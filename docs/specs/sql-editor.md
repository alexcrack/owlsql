# Spec: SQL editor (draft v0.1)

Status: **draft**, decisions 091–098 (see `../decisions.md`).
Covers: editor component architecture, highlighting, statement detection, execution keys,
completion engine, formatter, diagnostics, modes, performance targets and technical risks.

---

## 1. Architecture

```
SqlEditor.qml  (public QML API — the only thing the rest of the UI uses)
 ├─ Gutter            line numbers · fold markers · ▶ current statement · error icons
 ├─ TextArea          QQuickTextEdit-based text surface (v1 implementation)
 ├─ Decorations       C++ QQuickItem overlay: statement tint, bracket match, find hits,
 │                    error squiggles — painted for the visible range only
 ├─ CompletionPopup   list + detail pane (signature, column type, FK hint)
 └─ FindBar           find / replace, regex, case, whole word
          │
EditorController (C++, QML_ELEMENT) — owns all logic, works on the QTextDocument
 ├─ EditorHighlighter   own highlighter: KSyntaxHighlighting::AbstractHighlighter tokenizes,
 │                      formats applied in batches (decision 130) — not QSyntaxHighlighter
 ├─ StatementIndex      dialect.split() results, refreshed off-thread (debounced 150 ms)
 ├─ FoldingModel        foldable ranges: statements, BEGIN…END, $$ bodies, comment blocks
 ├─ BracketMatcher, AutoCloser
 ├─ CompletionEngine    (OwlCore, pure C++)
 ├─ Formatter           (OwlCore, pure C++)
 └─ DiagnosticsModel    server errors mapped to document ranges
```

**Swappability (decision 016):** all behaviour lives in `EditorController` and OwlCore. The
`TextArea` is only a rendering/input surface. A future scene-graph editor replaces the
TextArea + Decorations pair and binds to the same controller — `SqlEditor.qml`'s API doesn't change.

### 1.1 Public QML API (sketch)

```qml
SqlEditor {
    mode: SqlEditor.Sql            // Sql | Json | ReadOnlySql (DDL views, log details)
    dialect: session.dialect       // drives highlighting definition, splitting, completion
    catalog: session.catalogIndex  // null → no schema-aware completion
    readOnly: false
    text: buffer.content

    signal runRequested(var statements, int scope)   // Current | All | Selection
    function currentStatement()       // { text, start, end }
    function markError(position, message)
    function goTo(line, column)
    function formatSelectionOrStatement()
}
```

---

## 2. Highlighting

- KSyntaxHighlighting definitions: `SQL (PostgreSQL)`, `SQL (MySQL)`, `SQL` (SQLite) and `JSON`.
- Themes: own `Owl Dark` / `Owl Light` (decision 062); any bundled KSH theme selectable.
- **Own highlighter, not `QSyntaxHighlighter`** (decision 130). Measured in M1.2: on a laid-out
  `QQuickTextEdit`, `QSyntaxHighlighter` marks every re-highlighted block dirty separately and each
  mark relayouts everything below it — a full re-highlight of 1 MB took ~80 s. `EditorHighlighter`
  tokenizes with `KSyntaxHighlighting::AbstractHighlighter::highlightLine`, keeps the end-of-line
  `State` per block (`QTextBlockUserData`), sets formats with `QTextLayout::setFormats`, and marks
  the touched range dirty **once** per pass: ~0.5 s per MB for a full pass, linear.
- Edits: re-tokenize from the changed block forward until the stored state matches again, then one
  dirty mark for the touched range. Full passes (theme or dialect change) run as one batch with a
  busy indicator. Loading a file attaches the highlighter **before** setting the text, so the first
  pass happens before layout (~0.7 s per MB including load).
- The highlighter must leave the input-method preedit area alone (`QTextLayout::preeditAreaText`).
- Optional (post-v1): semantic tint for known table/column names from the catalog.

---

## 3. Statements and execution

- Statement boundaries come from `dialect.split()` (quotes, comments, PG dollar quotes, MySQL
  `DELIMITER`), recomputed on a document snapshot in a worker thread after edits.
- The statement under the cursor gets a subtle background tint and a ▶ gutter marker, so
  **Ctrl+Enter's target is always visible** (decision 092).

| Key | Action |
|---|---|
| Ctrl+Enter | Run current statement |
| F9 / Ctrl+Shift+Enter | Run all statements (sequentially, one result tab each) |
| Shift+F9 | Run selection (split into statements) |
| Esc (while running) / toolbar ■ | Cancel |
| Ctrl+Shift+F | Format selection or current statement |
| Ctrl+Space | Completion |
| Ctrl+F / Ctrl+H | Find / Replace |
| Ctrl+/ | Toggle line comment |
| Ctrl+[ / Ctrl+] | Fold / unfold at cursor |

(Cmd instead of Ctrl on macOS; all remappable, decision 081.)

Run-all: setting `editor.stopOnError` (default true). Before queuing, each statement passes the
safety guard (classification, prod confirmation) and placeholder prompt (Driver API §6.4, §9).

### 3.1 Large scripts

- Target: smooth editing (< 16 ms per keystroke) up to **2 MB** (decision 129, replaces 095).
  Measured in M1.2: typing cost grows with document size inside `QQuickTextEdit` itself —
  10 ms at 1 MB, 16 ms at 2 MB, ~40 ms at 5 MB, independent of highlighting and renderer.
- **2–5 MB** opens in the editor with a banner: *"Large script — editing may be slow. Run it
  without opening?"* offering Run SQL file…
- **Over 5 MB** is not opened in the editor: **Tools → Run SQL file…** streams the file through an
  incremental splitter on a dedicated connection, with progress, stop-on-error / continue, and
  errors listed with line numbers.
- Memory is ~60 MB per MB of text in the editor; another reason to stream large dumps.

---

## 4. Completion

### 4.1 Triggers (decision 093)
- Automatically after `.` and after 2 identifier characters in a context that expects an object
  or column (FROM, JOIN, INTO, UPDATE, SELECT list, WHERE, ON, ORDER/GROUP BY, SET).
- Ctrl+Space anywhere. Enter/Tab accepts, Esc closes. Typing continues to filter.

### 4.2 Engine (OwlCore `CompletionEngine`, pure C++, unit-tested)

```
input: statement text + cursor offset, IDialect, CatalogIndex snapshot
  1. tokenize current statement (dialect tokenizer)
  2. build scope: CTE names, FROM/JOIN table refs with aliases, subquery aliases (best effort)
  3. detect context at cursor:
       ObjectName      → schemas, tables, views, matviews, functions (set-returning)
       QualifiedMember → after "x.": columns of alias/table x, or objects of schema x
       ColumnName      → columns of in-scope tables (+ alias prefix), functions, keywords
       JoinCondition   → after "JOIN t a ON": FK-derived conditions first (decision 094)
       Keyword         → dialect keywords valid here
  4. rank: exact prefix > word-boundary/fuzzy match; in-scope columns > global;
     recently accepted items boosted
output: items { label, insertText, kind, detail (type / signature / FK), score }
```

- **CatalogIndex**: in-memory name index per session/database, fed by the catalog cache.
  Table names load with the tree; columns load lazily (async) the first time a table appears in
  scope — the popup updates when they arrive.
- **Functions**: built-in function list per dialect (shipped as data) + user functions from the
  catalog; signature hint in the detail pane.
- **JOIN conditions**: from FK metadata in both directions, e.g. `o.user_id = u.id`;
  multi-column FKs produce `AND`-joined conditions.
- Not in v1: snippet items in completion, `*` expansion.

---

## 5. Formatter

OwlCore `Formatter`, token-based (no full parser), deterministic, idempotent (decision 091).

Options (`settings.json`): `format.keywordCase` (upper | lower | preserve), `format.indent` (2 | 4),
`format.commaStyle` (trailing | leading), `format.clauseNewline` (true),
`format.maxColumnsPerLine` (1 = one column per line).

Rules: one major clause per line (SELECT, FROM, JOIN, WHERE, GROUP BY, HAVING, ORDER BY, LIMIT…),
indented column lists, `AND`/`OR` on new lines inside WHERE/ON, subqueries indented, comments and
string/dollar-quoted bodies untouched. Golden-file tests per dialect.

---

## 6. Diagnostics

- v1 = **server errors only** (decision 096): `DbError.position` (offset within the executed
  statement) is mapped back to the document → red squiggle + gutter icon + tooltip with message,
  detail and hint. Cleared when that statement is edited.
- Errors without a position mark the whole statement's gutter.
- No live linting in v1.

---

## 7. Editing features (v1)

- Code folding (statements, `BEGIN…END`, `$$` bodies, multi-line comments) — decision 097.
- Find / replace bar: regex, case-sensitive, whole word, replace all in selection.
- Bracket matching highlight; auto-close of `( [ { ' " \``; typing the closer skips over it;
  auto-close is one undo step.
- Line comment toggle, indent/outdent selection, duplicate line, move line up/down.
- Not in v1: multi-cursor / column selection, minimap, Vim mode.

---

## 8. Modes (decision 098)

| Mode | Used for |
|---|---|
| `Sql` | Query tabs, file-backed buffers |
| `ReadOnlySql` | DDL tabs, view definitions, Log/History statement details (copy, find, fold; no edits) |
| `Json` | JSON cell editor "Code" mode: JSON highlighting, Format / Minify, live validation |
| Routine body | `Sql` mode embedded in the routine editor; the editor shows only the body while the header (name, arguments, return type, language, volatility…) is a form; DDL is assembled by the dialect |

---

## 9. Performance targets

| Scenario | Target |
|---|---|
| Typing in a 2 MB script | < 16 ms key → frame (measured: 16 ms; 1 MB: 10 ms) |
| Statement index refresh, 2 MB | < 100 ms, off main thread |
| Completion popup | < 50 ms with columns cached |
| Open + highlight 1 MB file | < 700 ms (measured: ~0.6 s) |
| Full re-highlight (theme / dialect change), 1 MB | < 600 ms (measured: 0.52 s) |

---

## 10. M1.2 feasibility spike — results (2026-11-26)

Harness and raw numbers: branch `spike/m1.2-editor`, `spikes/editor/RESULTS.md`
(Linux, Qt 6.12.0, RelWithDebInfo; GPU and software renderer gave the same typing numbers).

| Risk | Result |
|---|---|
| 1. KSyntaxHighlighting on `QQuickTextDocument` | ✅ works; **`QSyntaxHighlighter` is unusable for full re-highlights** (1 MB: ~80 s, worse than quadratic). Own batch highlighter: 0.52 s per MB, linear → decision 130 |
| 2. Folding via hidden `QTextBlock`s | ✅ `QQuickTextEdit` collapses hidden blocks (`setVisible(false)` + one `markContentsDirty`) → decision 131 |
| 3. Overlay geometry | ✅ `positionToRectangle` 4–9 µs per call at 5 MB |
| 4. Typing latency / memory | ⚠️ 10 ms at 1 MB, 16 ms at 2 MB, ~40 ms at 5 MB (cost inside `QQuickTextEdit`); ~60 MB RAM per MB of text → decision 129 |
| 5. IME and undo grouping | ✅ auto-closed `()` as one `QTextCursor` edit block undoes in one step via `TextArea.undo()`; preedit / commit / undo work (synthetic events — confirm with a real input method when building the editor) |

**Decision (129): v1 editor stays on `TextArea`**, with the 2 MB smooth-editing target above. The
editor surface stays behind `EditorController` (§1), so a custom scene-graph editor can replace it
after 1.0 if large-script editing becomes a real need.

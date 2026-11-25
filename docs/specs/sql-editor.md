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
 ├─ SyntaxHighlighter   KSyntaxHighlighting::SyntaxHighlighter on QQuickTextDocument
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
- Highlighting is incremental per text block (QSyntaxHighlighter); no whole-document passes.
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

- Target: smooth editing up to **~5 MB** (decision 095).
- Bigger files are not opened in the editor: **Tools → Run SQL file…** streams the file through an
  incremental splitter on a dedicated connection, with progress, stop-on-error / continue, and
  errors listed with line numbers. Opening a file > 5 MB in the editor offers this instead.

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
| Typing in a 5 MB script | no visible latency (< 16 ms per keystroke on main thread) |
| Statement index refresh, 5 MB | < 100 ms, off main thread |
| Completion popup | < 50 ms with columns cached |
| Open 1 MB file | < 300 ms to first paint |

---

## 10. Risks → M1 spike

Before building the full editor, a 1–2 day spike on Qt 6.12 verifies:

1. KSyntaxHighlighting attached through `QQuickTextDocument::textDocument()` — highlighting and
   theme switching work live.
2. **Folding**: whether `QQuickTextEdit` honours hidden `QTextBlock`s. If not → fold by
   collapsing regions differently (e.g. a filtered display document) or defer folding.
3. Overlay geometry: `positionToRectangle` / block layout lookups fast enough for decorations on
   the visible range of a 5 MB document.
4. Typing latency and memory with a 5 MB document.
5. IME input and undo grouping with auto-close.

The spike's result decides whether v1 stays on TextArea or the custom editor moves earlier.

# Spec: Data grid (draft v0.1)

Status: **draft**, decisions 099–106 (see `../decisions.md`).
Covers: grid architecture, browsing/paging, filtering and sorting, selection and keyboard,
editing and staging, cell editors, FK navigation, clipboard, column state, record view.
Used by: table Data tabs, query result tabs, (read-only) history/log details.

---

## 1. Architecture

```
DataGrid.qml
 ├─ FilterBar            WHERE box + column-filter chips (table browsing only)
 ├─ HorizontalHeaderView name · type hint · sort indicator (1,2,3…) · filter button · resize/drag
 ├─ VerticalHeaderView   row numbers · staged-change markers
 ├─ TableView            cell delegates chosen by column kind (DelegateChooser)
 ├─ CellEditorHost       inline editor or popup editor for the current cell
 └─ Footer               pager / fetch state · staged-changes bar · selection stats
        │
GridController (C++, QML_ELEMENT)
 ├─ ResultModel : QAbstractTableModel   rows from QueryJob batches or browse pages
 ├─ SelectionModel                       rectangular ranges, current cell
 ├─ ChangeSet (RowChangeSet)             staged inserts / updates / deletes
 ├─ EditabilityResolver                  decides if/which cells are editable and the row key
 ├─ BrowseState                          page, WHERE text, column filters, sort keys
 └─ ColumnState                          order, visibility, widths (persisted per object)
```

Model roles: `display` (formatted text), `value` (owl::Value), `kind`, `isNull`, `isTruncated`,
`editState` (none | edited | inserted | deleted), `typeColor`, `isFk`.
Formatting happens in C++ (`Value::displayText`), never in delegates.

---

## 2. Browsing and paging (table Data tab)

- Data is fetched with `dialect.browseQuery()` on the session's `meta` worker:
  `SELECT … FROM t WHERE <filter> ORDER BY <sort> LIMIT 1000 OFFSET n`.
- **Pager** in the footer (decision 106): `◀  1–1000 of ~52 000  ▶`, page size from
  `grid.fetchSize` / session option, "Show all" for tables under a threshold (default 50 000 rows,
  with a warning). Row count uses the cheap estimate (pg_class.reltuples, information_schema,
  or `count(*)` for SQLite / small tables); exact count on click.
- Default order when no sort is chosen: primary key ascending (stable paging).
- Large values truncated server-side for browsing (Driver API §6.3).

**Query results** (query tabs) are not paged: they stream from the `QueryJob` and pause at 1000
rows with *Load more / Fetch all* (decision 045).

---

## 3. Filtering and sorting

- **WHERE box** (decision 034): raw SQL condition with completion of column names; Enter applies;
  history of recent filters per table; invalid filter → server error shown inline in the bar.
- **Column filter popup** (header ▾): operator list by type (=, ≠, <, >, between, contains,
  starts with, in list, is null, is not null, is empty JSON…) + value(s). Generates a SQL
  fragment that is **appended to the WHERE box** as a chip; the WHERE box remains the single
  source of truth (chips are views of parsed fragments; editing the text is always allowed).
- Quick filter from a cell: context menu *Filter = this value / ≠ this value / is NULL*.
- **Sorting**: click header = sort by this column (asc → desc → none); Shift+click adds secondary
  keys (decision 103). Server-side for browsing, client-side (stable) for fully-fetched query
  results; for paused query results sorting asks to fetch all first.

---

## 4. Selection and keyboard (decision 011, 099)

| Key | Action |
|---|---|
| Arrows / Home / End / PgUp / PgDn | Move current cell |
| Ctrl+Home / Ctrl+End | First / last cell |
| Shift + movement | Extend selection |
| Ctrl+A | Select all |
| Ctrl+Space / Shift+Space | Select column / row |
| *typing a character* | Start editing, replacing the value |
| Enter / F2 | Edit current cell (Enter on an edited cell commits and moves down) |
| Tab / Shift+Tab | Commit and move right / left |
| Esc | Cancel cell edit; second Esc clears selection |
| Ctrl+Enter | Open popup editor (large text, JSON, BLOB) |
| Ctrl+0 / Delete on selection | Set NULL (with confirmation for many cells) |
| Ctrl+Insert / Ctrl+D | Insert row / duplicate selected rows |
| Ctrl+Delete | Mark selected rows for deletion |
| Ctrl+S | Apply staged changes |
| Ctrl+Z / Ctrl+Shift+Z | Undo / redo staged edits |
| Ctrl+C / Ctrl+Shift+C | Copy / Copy as… |
| Ctrl+V | Paste |
| Ctrl+Click on FK cell | Jump to referenced row |
| Ctrl+R | Toggle record (transposed) view |

Footer shows selection stats: count, and for numeric selections sum / avg / min / max.

---

## 5. Editing and staging

### 5.1 Editability (decisions 100, 101)

`EditabilityResolver` decides per result:

1. **Table browsing**: editable if the table has a primary key, else a unique index whose columns
   are all NOT NULL. Otherwise read-only + banner *"No primary or unique key — rows can't be
   identified safely."*
2. **Query results**: a column is editable only if `ColumnInfo` reports a base table + column and
   that table's row key (PK or unique non-null) is fully present in the result. Non-editable
   cells show a lock tooltip explaining why.
3. Never editable: computed expressions, aggregates, views without key metadata,
   truncated values without key (Driver API §6.3), prod sessions in read-only mode.

### 5.2 Staging

- Edits go into a `RowChangeSet`: per row the key values, original values, new values, and
  state (`updated`, `inserted`, `deleted`). Visual markers per design system §3.3.
- Undo/redo works on staged edits.
- Footer bar: `3 changes staged  [Apply ⌘S] [Discard] [Preview SQL]`.
- Leaving the tab / closing / changing page with staged changes → prompt Apply / Discard / Cancel.
- Per-session option **"Apply rows immediately"**: a row is applied when the cursor leaves it
  (HeidiSQL feel), still via the same ChangeSet path.

### 5.3 Apply (decision 102)

- `dialect.rowChanges()` renders statements; **Preview SQL** shows exactly those.
- All statements run in **one transaction** (or inside the user's open manual transaction —
  then nothing is committed automatically). First error → rollback, the failing row is
  highlighted with the error, all changes stay staged.
- `UPDATE … SET <changed cols> WHERE <key> = <original key values>`; deletes by key.
- After apply, affected rows are refreshed: `RETURNING *` where supported (PostgreSQL, SQLite,
  MariaDB), otherwise re-select by key / last insert id (MySQL).
- Safety guards apply (prod confirmation shows the preview).
- Post-v1 option: optimistic concurrency check (include original values of edited columns in
  WHERE and report "row changed by someone else").

---

## 6. Cell editors (decision 104)

| Column kind | Inline editor | Popup (Ctrl+Enter) |
|---|---|---|
| Text, numbers, uuid, unknown | text field (numbers validated) | Large text popup (multi-line, word wrap, line count) |
| Boolean | tri-state toggle (✓ / ✗ / NULL) | — |
| Enum / MySQL SET | dropdown / checklist | — |
| Date, time, timestamp(tz) | masked text field + calendar/time picker button; tz shown | — |
| JSON / JSONB | text field (single line) | **JSON editor**: Code ⇄ Constructor (decision 022) |
| Bytes / BLOB | `0x…` preview, read-only inline | **BLOB editor**: hex view, image preview (PNG/JPEG/GIF/WebP/SVG), text view, load from file, save to file |
| FK column | text field + lookup button | **FK lookup picker** (decision 035) |
| Array (PG) | text literal `{a,b}` | Large text popup |

Every editor offers *Set NULL* and *Set DEFAULT* (when the column has a default).

---

## 7. Foreign keys (decision 035)

- FK cells show a small link glyph on hover. **Ctrl+Click** (or context menu *Go to referenced
  row*) opens the parent table tab filtered to that key (`WHERE id = 42`), or focuses it if open.
- **Lookup picker**: searchable popup listing parent rows — key + a "display column" guess
  (first unique text column, e.g. `name`, `email`, `title`), with search across text columns,
  paged 50 at a time. Picking a row writes the key.
- Show referencing rows (children) is not in v1.

---

## 8. Clipboard and bulk operations (decision 105)

- **Copy** (Ctrl+C): TSV of the selection (no header). **Copy as…**: CSV (with header),
  JSON (array of objects), Markdown table, SQL `INSERT` statements, SQL `WHERE col IN (…)`,
  and *Copy with headers*. NULL copies as empty in TSV/CSV, `null` in JSON, `NULL` in SQL.
- **Paste** (Ctrl+V) TSV/CSV from spreadsheets: into the selection starting at the current cell;
  if the block extends past the last row → offer to insert new rows. Values are parsed per
  column type; failures listed before staging.
- **Bulk set**: selected cells → value / NULL / DEFAULT (staged).
- **Insert row** (defaults pre-filled, identity/serial left empty), **duplicate rows** (key and
  identity columns cleared).

---

## 9. Columns (decisions 103)

- Hide / show via column chooser (searchable), drag to reorder, resize, double-click border to
  auto-fit (sampled rows). Persisted per object in `workspace_tab.state` / `ui_state`.
- Multi-column sort (§3).
- Freeze columns: not in v1.
- **Record view** (Ctrl+R): current row shown vertically (column name · type · value), editable
  with the same editors; ◀ ▶ moves between rows. Best for wide tables.

---

## 10. Display

- NULL: dimmed italic `NULL`; empty string: empty cell (decision 059).
- Numbers right-aligned, tabular figures; booleans ✓ / ✗; truncated values end with `…` + badge.
- Per-type value colours behind the `grid.colorByType` toggle; zebra and vertical lines toggles.
- Long text is shown on one line (newlines as `↵`), tooltip with the first ~500 chars.

---

## 11. Performance targets

| Scenario | Target |
|---|---|
| Scroll 1000 rows × 100 columns | 60 fps, delegates recycled (TableView reuse) |
| Append a 1000-row batch | < 30 ms on main thread |
| Apply 500 staged changes | single transaction, progress if > 1 s |
| Grid memory, 100 000 rows × 20 cols fetched | < 300 MB (compact Value storage) |

---

## 12. Testing

- Unit: EditabilityResolver cases (PK, unique, none, joins, expressions), ChangeSet → SQL per
  dialect (golden files), filter chip ↔ WHERE text round-trip, paste parsing.
- Integration: apply/rollback per engine, RETURNING vs re-select, FK lookup on fixture schema.
- QML: keyboard navigation and editing flows on a mock model.

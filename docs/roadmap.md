# Roadmap

No dates — a milestone ships when its exit criteria are met.

**Keeping it current:** the PR that finishes an item ticks its box (`- [x]`) and adds the PR number.
Items that move to another milestone stay unticked with a note pointing to where they went.

## M0 — Foundation ✅ (2026-11-26)
- [x] Repo, CMake + presets, vcpkg manifest (empty until M1) — #1
- [x] GitHub Actions matrix: build + tests on Linux / Windows / macOS, format + lint — #2
- [x] Theme tokens singleton, Light / Dark / System, Inter + JetBrains Mono, Tabler icons — #3
- [x] `OwlSql.Style` controls (buttons, inputs, tabs, menus, scrollbars, splitters…) + Component Gallery — #4
- [ ] Styled tree rows → moved to M1.9 (object tree)
- [x] App shell: fixed panes (sidebar / tabs / bottom panel), menu bar, status bar, command registry — #5
- [ ] Settings dialog skeleton → moved to M3 (settings are edited in `settings.json` until then)
- [x] Local SQLite app store, `settings.json`, `keybindings.json`, single instance — #6
- [x] i18n plumbing; placeholder logo — #6, #5
- [x] **Exit:** themed empty shell runs on all 3 OSes with CI green

## M1 — Read-only client
Goal: daily-usable for browsing and querying PostgreSQL and SQLite. PRs in order; "needs" lists
the PRs that must land first.

- [x] **1.1 Dependencies + CI** — vcpkg (pinned baseline, binary cache) for libpq 18.4 and sqlite3
  3.54; QtKeychain 0.18 + KSyntaxHighlighting 6.32 built against our Qt (decisions 124, 126–128);
  PostgreSQL 18 in Docker on the Linux CI job (decision 125); our own vcpkg checkout in CI
  (Visual Studio's `VCPKG_ROOT` is overridden). — #9
- [x] **1.2 Editor spike** — TextArea + KSyntaxHighlighting on Qt 6.12 measured
  (`specs/sql-editor.md` §10): v1 stays on TextArea with a 2 MB smooth-editing target, own batch
  highlighter instead of QSyntaxHighlighter, folding works (decisions 129–132). — #10
- [x] **1.3 OwlCore values** — `Value` (18 kinds, 40 bytes), `ColumnInfo`, `RowBatch`, `DbError`,
  `ServerMessage`, `Result<T>` (tl::expected), CBOR wire format v1 with round-trip tests
  (decisions 133–136). — #11
- [x] **1.4 Driver API + execution** — `IDriver` / `IConnection` / `IResultCursor` /
  `ICancelHandle`, `DriverRegistry` (plugins, core API check), `ConnectionWorker` threads,
  `QueryJob` (pause / fetchMore / fetchAll / cancel), scriptable fake driver incl. real plugin
  files (decisions 137–140). — #12
- [x] **1.5 SQLite driver** — sqlite3 plugin (types, bound parameters, truncation, error positions,
  cancel, read-only guard), dialect layer (tokenizer, split, classify, placeholders, quoting),
  driver conformance suite (decisions 141–145). — #13
- [x] **1.6 PostgreSQL driver** — libpq plugin: chunked rows, cancel, notices, type mapping, SSL,
  sockets; dialect (dollar quotes, nested comments, `$n`, SELECT INTO, EXPLAIN (ANALYZE)); conformance
  + integration tests vs PG 13 and 18 (decisions 146–153). — #14
- [ ] **1.7 Catalog** — `ICatalog` for PG / SQLite (tree nodes, TableDef, DDL text), `CatalogCache`
  with invalidation. _needs: 1.5, 1.6_
- [ ] **1.8 Sessions** — migration 0002 (`folder`, `session`), keychain secrets, session manager in
  the Welcome tab, `ConnectionFormSchema` spec, URL / `.pgpass` import, safety levels.
  _needs: 1.4, 1.1_
- [ ] **1.9 Object tree** — sidebar tree over the catalog: styled tree rows, lazy loading, filter,
  full PG depth, icons, refresh. _needs: 1.7, 1.8_
- [ ] **1.10 Data grid (read-only)** — `ResultModel`, object tab Data view, browse paging, WHERE box
  + column filters, sorting, keyboard navigation, NULL / truncation display, copy. _needs: 1.7_
- [ ] **1.11 SQL editor + results** — TextArea editor with `EditorHighlighter` (decisions 129–131), run keys, statement tint, result
  tabs, Messages, error positions, placeholder prompt, safety guards. _needs: 1.2, 1.10_
- [ ] **1.12 Log + history** — Log panel (all statements, filters), `history.db` with FTS5 search
  and retention. _needs: 1.4_
- [ ] **Exit:** browse and query real PostgreSQL and SQLite databases on all 3 OSes; CI runs
  PostgreSQL integration tests; no known crash or data-loss bug.

### Decisions made for M1 (2026-11-26)
- **D1 → decision 124:** vcpkg only for C libraries (libpq, sqlite3, later libmariadb, libssh2).
  QtKeychain and KSyntaxHighlighting are built from pinned source tags with CMake `FetchContent`
  against our Qt (KSyntaxHighlighting needs extra-cmake-modules and Perl at build time).
- **D2 → decision 125:** server integration tests (PostgreSQL now, MariaDB/MySQL in M2) run on the
  Linux CI job in Docker service containers; SQLite and unit tests run on every OS.

## Carried over from M0
- [ ] Windows CI pins aqtinstall to an unreleased commit (Qt 6.12 layout) — switch back to a
  release (> 3.3.0) when available (`.github/workflows/ci.yml` TODO).
- [ ] Merged feature branches remain on GitHub — delete them, or enable "Automatically delete head
  branches".
- [ ] Crash reporting library — decide before 1.0 (decision 070).

## M2 — Editing
- [ ] Grid editing with ChangeSet + preview SQL; type-specific cell editors; JSON editor
  (code / constructor).
- [ ] FK jump + lookup picker.
- [ ] Transactions: auto-commit toggle, open-TX indicator; prod safety guards.
- [ ] Table structure editor with DDL preview.
- [ ] MySQL / MariaDB driver.
- [ ] SQLite column origin metadata (`SQLITE_ENABLE_COLUMN_METADATA` via a vcpkg overlay port) so
  SQLite query results can be edited (from M1.5).
- [ ] **Exit:** can safely modify data and schema on all Tier-1 engines.

## M3 — Productivity
- [ ] Context-aware completion.
- [ ] Command palette.
- [ ] Session / workspace restore — layout and open tabs are restored since #6; still to do:
  query tabs, unsaved buffers, per-tab state.
- [ ] Settings dialog (moved from M0).
- [ ] SSH tunnels.
- [ ] Export (CSV, JSON, SQL INSERT, XLSX) / import (CSV with column mapping).
- [ ] Snippets, find text across database.
- [ ] Connection importers (HeidiSQL, pgAdmin 4, DBeaver, MySQL Workbench).

## M4 — Object editors → 1.0
- [ ] Views / matviews, functions / procedures / triggers, users / roles / grants,
  sequences / types / extensions.
- [ ] Packaging: .deb, .rpm, Snap; signed Windows / macOS builds; update check + auto-updater;
  opt-in crash reports.
- [ ] Docs site, polish, accessibility pass.
- [ ] **Exit:** OwlSql 1.0.

## Post-1.0
- [ ] EXPLAIN visualizer
- [ ] Activity dashboard
- [ ] ERD
- [ ] Schema diff
- [ ] Backup / restore
- [ ] SQL Server, DuckDB, ClickHouse, ODBC drivers
- [ ] Public plugin SDK
- [ ] User theme files
- [ ] Docking panels
- [ ] OwlSql Gateway + WASM build
- [ ] Optional AI features
- [ ] PL/pgSQL debugger
- [ ] Vim mode

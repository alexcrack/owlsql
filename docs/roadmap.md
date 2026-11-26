# Roadmap (draft)

No dates — a milestone ships when its exit criteria are met.

## M0 — Foundation
- Repo, CMake + presets, vcpkg manifest, GitHub Actions matrix (build + tests on 3 OSes).
- `OwlSql.Style`: Theme tokens singleton, Light/Dark/System, custom Controls style (buttons, inputs,
  tabs, tree rows, menus, scrollbars, splitters), Tabler icon component, Inter + JetBrains Mono.
- App shell: fixed panes (tree / tabs / bottom panel), menus, status bar, settings dialog skeleton.
- Local SQLite app store; i18n plumbing; placeholder logo.
- **Exit:** themed empty shell runs on all 3 OSes with CI green.
- Status: **complete** (2026-11-26) — scaffold (#1), CI (#2), theme (#3), style + gallery (#4),
  shell + command registry (#5), app data / settings / keybindings / i18n / single instance (#6).

## M1 — Read-only client
Goal: daily-usable for browsing and querying PostgreSQL and SQLite.

### Planned PRs (in order)
| # | PR | Content | Depends on |
|---|---|---|---|
| 1.1 | Dependencies + CI | vcpkg in CI (pinned baseline, binary cache) for libpq, sqlite3; QtKeychain + KSyntaxHighlighting via FetchContent (decision 124); PostgreSQL service container on the Linux job (decision 125) | — |
| 1.2 | Editor spike | Throwaway branch: TextArea + KSyntaxHighlighting on Qt 6.12 — highlighting, folding, overlays, 5 MB typing latency, IME/undo (`specs/sql-editor.md` §10). Outcome recorded as a decision; decides editor approach | 1.1 |
| 1.3 | OwlCore values | `Value`, `ColumnInfo`, `RowBatch`, `DbError`, `Result<T>` (tl::expected), CBOR round-trips | — |
| 1.4 | Driver API + execution | `IDriver`/`IConnection`/`IResultCursor`/`ICancelHandle`, plugin loading, `ConnectionWorker` threads, `QueryJob` facade (pause/fetchMore/cancel), fake driver for tests | 1.3 |
| 1.5 | SQLite driver | sqlite3 plugin, dialect basics (quote, split, classify, placeholders), conformance suite v1 | 1.4 |
| 1.6 | PostgreSQL driver | libpq plugin: chunked rows, cancel, notices, type mapping, SSL, sockets; integration tests vs PG 13 + latest | 1.4, 1.1 |
| 1.7 | Catalog | `ICatalog` for PG/SQLite (tree nodes, TableDef, DDL text), `CatalogCache` with invalidation | 1.5, 1.6 |
| 1.8 | Sessions | Migration 0002 (`folder`, `session`), keychain secrets, session manager in the Welcome tab, `ConnectionFormSchema` spec, URL / `.pgpass` import, safety levels | 1.4, 1.1 |
| 1.9 | Object tree | Sidebar tree over the catalog: lazy loading, filter, full PG depth, icons, refresh | 1.7, 1.8 |
| 1.10 | Data grid (read-only) | `ResultModel`, object tab Data view, browse paging, WHERE box + column filters, sorting, keyboard navigation, NULL/truncation display, copy | 1.7 |
| 1.11 | SQL editor + results | Editor per spike outcome, run keys, statement tint, result tabs, Messages, error positions, placeholders prompt, safety guards | 1.2, 1.10 |
| 1.12 | Log + history | Log panel (all statements, filters), `history.db` with FTS5 search and retention | 1.4 |

**Exit:** browse and query real PostgreSQL and SQLite databases on all 3 OSes; CI runs PostgreSQL
integration tests; no known crash or data-loss bug.

### Decisions made for M1 (2026-11-26)
- **D1 → decision 124:** vcpkg only for C libraries (libpq, sqlite3, later libmariadb, libssh2).
  QtKeychain and KSyntaxHighlighting are built from pinned source tags with CMake `FetchContent`
  against our Qt (KSyntaxHighlighting needs extra-cmake-modules and Perl at build time).
- **D2 → decision 125:** server integration tests (PostgreSQL now, MariaDB/MySQL in M2) run on the
  Linux CI job in Docker service containers; SQLite and unit tests run on every OS.

## Carried over from M0
- Windows CI pins aqtinstall to an unreleased commit (Qt 6.12 layout) — switch back to a release
  (> 3.3.0) when available (`.github/workflows/ci.yml` TODO).
- Merged feature branches remain on GitHub — delete, or enable "Automatically delete head branches".
- Crash reporting library — decide before 1.0 (decision 070).
- `ui_state` is only a key-value table; sessions and workspace tables come as migrations (decision 119).

## M2 — Editing
- Grid editing with ChangeSet + preview SQL; type-specific cell editors; JSON editor (code/constructor).
- FK jump + lookup picker.
- Transactions: auto-commit toggle, open-TX indicator; prod safety guards.
- Table structure editor with DDL preview.
- MySQL/MariaDB driver.
- **Exit:** can safely modify data and schema on all Tier-1 engines.

## M3 — Productivity
- Context-aware completion.
- Command palette; session/workspace restore.
- SSH tunnels.
- Export (CSV, JSON, SQL INSERT, XLSX) / import (CSV with column mapping).
- Snippets, find text across database.
- Connection importers (HeidiSQL, pgAdmin 4, DBeaver, MySQL Workbench).

## M4 — Object editors → 1.0
- Views/matviews, functions/procedures/triggers, users/roles/grants, sequences/types/extensions.
- Packaging: .deb, .rpm, Snap; signed Windows/macOS builds; update check + auto-updater; opt-in crash reports.
- Docs site, polish, accessibility pass.
- **Exit:** OwlSql 1.0.

## Post-1.0
EXPLAIN visualizer · activity dashboard · ERD · schema diff · backup/restore · SQL Server, DuckDB,
ClickHouse, ODBC drivers · public plugin SDK · user theme files · docking · OwlSql Gateway + WASM
build · optional AI features · PL/pgSQL debugger · Vim mode.

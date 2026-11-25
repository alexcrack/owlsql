# Roadmap (draft)

No dates — a milestone ships when its exit criteria are met.

## M0 — Foundation
- Repo, CMake + presets, vcpkg manifest, GitHub Actions matrix (build + tests on 3 OSes).
- `OwlSql.Style`: Theme tokens singleton, Light/Dark/System, custom Controls style (buttons, inputs,
  tabs, tree rows, menus, scrollbars, splitters), Tabler icon component, Inter + JetBrains Mono.
- App shell: fixed panes (tree / tabs / bottom panel), menus, status bar, settings dialog skeleton.
- Local SQLite app store; i18n plumbing; placeholder logo.
- **Exit:** themed empty shell runs on all 3 OSes with CI green.
- Status (2026-11-25): scaffold (#1), CI (#2), theme (#3), style + gallery (#4), shell + command
  registry (#5) done. Remaining: local SQLite app store + `settings.json` (UI state persistence),
  i18n plumbing (PR 6).

## M1 — Read-only client
- OwlCore: Driver API, worker threads, async query + cancel, result streaming, Catalog, Dialect.
- Drivers: PostgreSQL, SQLite.
- Session manager: folders, safety levels, keychain, SSL, sockets, URL import.
- Object tree (full PG depth, filterable, lazy loading).
- Data grid (read-only): paging, sorting, WHERE box + column filters, keyboard navigation.
- SQL editor: highlighting, gutter, HeidiSQL run keys, multiple result tabs, Messages panel.
- Query log panel; persistent history.
- **Exit:** daily-usable for browsing and querying PostgreSQL/SQLite.

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

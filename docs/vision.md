# OwlSql — Vision

> HeidiSQL's speed and directness + pgAdmin's depth, wrapped in a modern shell.

OwlSql is a free, open-source (GPL-3.0) desktop database client built with Qt 6.12 and QML.
It targets developers and DBAs who want a fast, keyboard-friendly tool that still understands
the full object model of the database they work with.

## Audience & distribution

- **Public open-source product**, GPL-3.0.
- Platforms from day one: **Linux, Windows, macOS**. **WebAssembly** is planned later (see architecture).
- **Linux**: free public binaries — native `.deb` / `.rpm` and **Snap**.
- **Windows / macOS**: binaries for paid subscribers, sold via a store/reseller
  (Paddle / Lemon Squeezy / similar). No license checks in the app — source stays fully GPL.
- Updates: the app checks the GitHub Releases API and notifies. Linux updates via package managers;
  Windows/macOS builds include an auto-updater.
- Opt-in crash reports only. No usage telemetry.
- UI language: English, fully i18n-ready (`qsTr`, Qt Linguist pipeline).
- Pace: no deadline, quality first.

## Databases

| Tier | Engines | Notes |
|---|---|---|
| 1 (v1) | PostgreSQL, MySQL / MariaDB, SQLite | Native drivers, full catalog + editors |
| 2 (post-1.0) | SQL Server, DuckDB, ClickHouse | Native drivers |
| Fallback | Anything via ODBC | Basic browse/query only |

## Feature pillars

### From HeidiSQL
- Session manager with folders, quick connect.
- Fast, editable data grid with staged changes, WHERE box + column filters.
- One-screen table editor (columns, indexes, FKs, checks).
- Query log panel showing every statement the client sends.
- Export / import, snippets, SSH tunnels, find text across a database.
- HeidiSQL-style run keys: Ctrl+Enter (current statement), F9 (all), Shift+F9 (selection).

### From pgAdmin
- Full-depth, filterable object tree (all PG object categories; empty categories & system schemas hideable).
- DDL preview for every change before it is applied.
- Editors for views/materialized views, functions/procedures/triggers, users/roles/grants,
  sequences/types/extensions.
- Post-1.0: graphical EXPLAIN, activity dashboard, ERD, schema diff, backup/restore, PL debugger.

### OwlSql's own
- **Command palette** (Ctrl+K): actions, objects, history.
- **Session restore**: tabs, unsaved queries, scroll positions.
- **Keyboard-first grid**: spreadsheet-like navigation and editing.
- **Per-session safety level** (dev / staging / prod): prod gets a colored frame,
  write confirmation, blocks `UPDATE`/`DELETE` without `WHERE`.
- **Transaction awareness**: auto-commit by default, manual mode one click away,
  visible "transaction open" indicator.
- **JSON editor** with two switchable modes: *code* (validated, formatted) and
  *constructor* (visual tree: add/remove/retype nodes). For `json`/`jsonb`/MySQL `JSON`.
- **FK navigation**: jump to the referenced row; lookup picker when editing an FK cell.
- **Persistent searchable history** (local SQLite, 90-day default retention, prod sessions may opt out).
- **Connection importers**: HeidiSQL, pgAdmin 4, DBeaver, MySQL Workbench; connection URLs, `.pgpass`, `.my.cnf`.
- Optional AI features later (opt-in, bring your own key) — not in v1.

## Look & feel

- **Modern IDE-like**: dense, flat, 1px dividers, ~24px rows, accent color (Fleet / VS Code feel).
- **Fixed pane layout**: object tree (left), tabbed workspace (center), bottom panel (Log / History / Messages).
  Panes can be collapsed and resized. No docking in v1.
- **One tree** with every open connection as a root node.
- Themes: Light / Dark / System, a few built-in themes; token format kept stable for user theme files later.
- **Tabler Icons** (SVG, tinted per theme). Bundled **Inter** (UI) + **JetBrains Mono** (editor, grid).
- Logo: placeholder SVG owl for now, proper logo later.

## Non-goals for v1
Docking panels, public plugin SDK, AI features, WASM build, Tier-2 drivers, EXPLAIN visualizer,
activity dashboard, ERD, schema diff.

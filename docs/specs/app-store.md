# Spec: App data & session store (draft v0.1)

Status: **draft**, decisions 083–090 (see `../decisions.md`).
Covers: where OwlSql keeps its own data, the SQLite schemas, settings file, secrets, session
sharing, workspace restore, migrations, single-instance behaviour.

---

## 1. Files and locations

Resolved through a single `AppPaths` class (so portable mode can be added later, decision 072).

| File | Location (`QStandardPaths`) | Contents |
|---|---|---|
| `settings.json` | `AppConfigLocation` | User preferences (human-editable) |
| `keybindings.json` | `AppConfigLocation` | Shortcut overrides only |
| `owlsql.db` | `AppDataLocation` | Sessions, folders, snippets, workspace/tabs, scratch buffers, UI state |
| `history.db` | `AppDataLocation` | Query history (large; can be wiped/rotated separately) |
| `backups/` | `AppDataLocation` | Pre-migration copies of the databases (last 3 kept) |
| `themes/` | `AppDataLocation` | Reserved for user theme files (post-1.0) |
| `logs/` | `AppDataLocation` | App diagnostic log (rotating) — not the SQL Log panel |

Typical paths: Linux `~/.config/OwlSql/`, `~/.local/share/OwlSql/`; Windows `%APPDATA%\OwlSql\`;
macOS `~/Library/Application Support/OwlSql/`. Snap: inside `$SNAP_USER_DATA`.

Both databases: SQLite, `journal_mode=WAL`, `foreign_keys=ON`, opened only by the
main-thread `AppDatabase` service (plus a dedicated history writer thread). They use QtSql's bundled
SQLite driver — this is OwlSql's own local storage, not a user-database driver (decision 118).

`--data-dir <dir>` puts everything under `<dir>/config` and `<dir>/data` (development, tests).

---

## 2. Secrets

- Stored only in the OS keychain via QtKeychain (decision 013). Never in the databases,
  `settings.json` or exports.
- Key format: service `OwlSql`, account `session/<session-uuid>/<secret>` where `<secret>` is
  `password`, `ssh-password`, `ssh-key-passphrase`, `ssl-key-passphrase`.
- Session row stores only `secret_mode` per secret: `keychain` | `ask` | `none`.
- If the keychain is unavailable (headless Linux without Secret Service), the app falls back to
  `ask` and shows a one-time banner.

---

## 3. `owlsql.db` schema

> **Implemented (M0):** migration `0001_init.sql` creates only `ui_state`. The other tables below
> are the target design; each arrives as a new migration with the feature that uses it (M1+).

```sql
CREATE TABLE folder (
  id          TEXT PRIMARY KEY,               -- uuid
  parent_id   TEXT REFERENCES folder(id) ON DELETE CASCADE,
  name        TEXT NOT NULL,
  position    INTEGER NOT NULL DEFAULT 0,
  expanded    INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE session (
  id             TEXT PRIMARY KEY,            -- uuid; also the keychain key
  folder_id      TEXT REFERENCES folder(id) ON DELETE SET NULL,
  position       INTEGER NOT NULL DEFAULT 0,
  name           TEXT NOT NULL,
  driver_id      TEXT NOT NULL,               -- 'postgres' | 'mysql' | 'sqlite'
  params         TEXT NOT NULL,               -- JSON: ConnectionParams minus secrets
  ssh            TEXT,                        -- JSON: tunnel config or NULL
  safety_level   TEXT NOT NULL DEFAULT 'dev', -- dev | staging | prod
  color          TEXT,                        -- tag colour name or NULL (= level default)
  options        TEXT NOT NULL DEFAULT '{}',  -- JSON: see §3.1
  notes          TEXT,
  secret_modes   TEXT NOT NULL DEFAULT '{}',  -- JSON: {"password":"keychain",...}
  created_at     TEXT NOT NULL,
  updated_at     TEXT NOT NULL,
  last_used_at   TEXT,
  imported_from  TEXT                         -- 'heidisql' | 'pgadmin' | 'dbeaver' | 'workbench' | 'json'
);

CREATE TABLE snippet (
  id          TEXT PRIMARY KEY,
  name        TEXT NOT NULL,
  body        TEXT NOT NULL,
  driver_id   TEXT,                           -- NULL = any engine
  tags        TEXT,
  created_at  TEXT NOT NULL,
  updated_at  TEXT NOT NULL
);

CREATE TABLE buffer (                         -- scratch query tabs + file-backed tabs
  id            TEXT PRIMARY KEY,
  title         TEXT NOT NULL,
  content       TEXT NOT NULL DEFAULT '',     -- autosaved (debounced 1 s)
  file_path     TEXT,                         -- NULL = scratch; else linked .sql file
  file_mtime    TEXT,                         -- to detect outside changes
  session_id    TEXT REFERENCES session(id) ON DELETE SET NULL,
  database      TEXT, schema TEXT,
  cursor_pos    INTEGER, scroll_y REAL,
  param_values  TEXT,                         -- JSON: remembered placeholder values
  updated_at    TEXT NOT NULL
);

CREATE TABLE workspace_tab (                  -- restore order and per-tab UI state
  id          TEXT PRIMARY KEY,
  position    INTEGER NOT NULL,
  kind        TEXT NOT NULL,                  -- welcome | query | object | ...
  buffer_id   TEXT REFERENCES buffer(id) ON DELETE CASCADE,
  session_id  TEXT REFERENCES session(id) ON DELETE CASCADE,
  object_path TEXT,                           -- JSON ObjectPath for object tabs
  state       TEXT NOT NULL DEFAULT '{}',     -- JSON: inner tab, grid scroll, filters, column widths...
  is_active   INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE ui_state (                       -- splitter sizes, tree expansion, window geometry
  key   TEXT PRIMARY KEY,
  value TEXT NOT NULL                         -- JSON
);
```

### 3.1 `session.options` JSON

```json
{
  "startupScript": "SET search_path TO app, public;",
  "defaultDatabase": "app_db",
  "defaultSchema": "public",
  "visibleDatabases": ["app_db", "analytics"],    // empty = all
  "fetchSize": 1000,
  "queryTimeoutSec": null,
  "keepaliveMin": null,
  "singleConnection": false,
  "readOnly": false
}
```

Every key is optional; missing keys fall back to `settings.json` defaults. The startup script runs
on **every** new physical connection of the session (meta, per-tab, per-database) and is shown in
the Log as `Internal`.

---

## 4. `history.db` schema (v1)

```sql
CREATE TABLE entry (
  id            INTEGER PRIMARY KEY,
  executed_at   TEXT NOT NULL,
  session_id    TEXT,                         -- not a FK (sessions live in the other file)
  session_name  TEXT NOT NULL,                -- denormalised: survives session deletion
  driver_id     TEXT NOT NULL,
  database      TEXT, schema TEXT,
  sql           TEXT NOT NULL,
  duration_ms   INTEGER,
  rows          INTEGER,
  status        TEXT NOT NULL,                -- ok | error | cancelled
  error         TEXT
);
CREATE INDEX entry_time ON entry(executed_at);
CREATE INDEX entry_session ON entry(session_id, executed_at);

CREATE VIRTUAL TABLE entry_fts USING fts5(sql, content='entry', content_rowid='id');
-- + triggers keeping entry_fts in sync
```

- Writes go through a dedicated writer thread (batched) so logging never blocks the UI.
- Retention: default 90 days (`history.retentionDays`), pruned at startup and daily; prod sessions
  can opt out of history entirely (`history.skipProd` or per-session option).
- Identical consecutive statements in the same session are collapsed (count + last time).

---

## 5. `settings.json`

- Strict JSON (no comments). Only **non-default** values are written; defaults are compiled in
  (`Settings::schema()`), so the file stays small and readable.
- Implemented keys (M0): `theme.mode` (`system` | `light` | `dark`), `ui.reduceMotion`. The example
  below shows the planned set.
- Schema-driven: each key has type, default, description, allowed values. The Settings UI and
  validation are generated from it; `Help → Open settings.json` opens the file in an OwlSql tab.
- Unknown keys are kept and reported as warnings; invalid values fall back to defaults with a
  banner. File changes on disk are picked up live.

```json
{
  "theme.mode": "system",              // system | light | dark
  "theme.accent": "amber",
  "editor.fontSize": 13,
  "editor.tabSize": 4,
  "editor.highlightTheme": "auto",     // auto = matches UI theme
  "grid.fetchSize": 1000,
  "grid.previewLimitKb": 64,
  "grid.zebra": true,
  "grid.verticalLines": true,
  "grid.colorByType": false,
  "history.retentionDays": 90,
  "history.skipProd": false,
  "connection.keepaliveMin": null,
  "updates.check": true,
  "crashReports.enabled": false        // opt-in, asked once on first run
}
```

`keybindings.json` holds only overrides: `[{ "command": "query.runStatement", "key": "Ctrl+Return" }]`.

---

## 6. Session sharing (export / import)

- **Export**: select sessions or a folder → `*.owlsql-sessions.json`. Secrets are never exported;
  `secret_modes` become `ask` on import.

```json
{
  "format": "owlsql-sessions",
  "version": 1,
  "exportedAt": "2026-11-25T20:00:00Z",
  "folders": [{ "id": "…", "parentId": null, "name": "Work" }],
  "sessions": [{
    "id": "…", "folderId": "…", "name": "prod-pg", "driverId": "postgres",
    "params": { "host": "db.internal", "port": 5432, "user": "app", "database": "app_db",
                "ssl": { "mode": "verify-full" } },
    "ssh": null, "safetyLevel": "prod", "color": "red",
    "options": { "startupScript": "SET search_path TO app;" }, "notes": "VPN required"
  }]
}
```

- **Import**: merge by `id`; on conflict ask *keep both / replace / skip*. File paths (SSL certs,
  SSH keys) are kept as-is and flagged if missing.
- The same import pipeline is used by the third-party importers (HeidiSQL, pgAdmin 4, DBeaver,
  MySQL Workbench): each importer converts to this JSON model first.

---

## 7. Workspace restore

- On every tab change / debounced UI change, `workspace_tab` and `ui_state` are updated.
- On startup: tabs, tree expansion and layout are restored **immediately**; nothing connects.
  A session connects lazily the first time a tab or tree node needs it.
  Prod sessions never connect without a user action, even lazily from restore — their tabs show
  a "Connect to prod-pg" placeholder.
- Scratch buffers are autosaved (1 s debounce) → content is never lost, even on crash.
- File-backed buffers: normal save semantics (dirty dot, Ctrl+S); external modification →
  banner "File changed on disk — Reload / Keep mine".

---

## 8. Migrations

- Numbered SQL files embedded as Qt resources: `migrations/owlsql/0001_init.sql`, `…/history/0001_init.sql`.
- Version tracked in `PRAGMA user_version`.
- Before migrating: copy the file to `backups/owlsql-v<old>-<timestamp>.db` (keep last 3).
- Each migration runs in a transaction; failure → restore from backup, show error, quit.
- An older OwlSql opening a newer database refuses with "created by a newer OwlSql (vX)"
  instead of touching it.

---

## 9. Single instance

- `QLocalServer` named per user (`OwlSql-<user-hash>`). A second launch connects, forwards its
  arguments (`owlsql file.sql`, `owlsql postgres://…`, `owlsql --session <name>`), and exits; the
  running instance raises its window and handles them.
- If the socket is stale (crash), the new process removes it and becomes the primary.
- `--new-instance` for development only (uses a separate data directory via `--data-dir`).

---

## 10. Testing

- Migrations: apply all from empty, plus fixture databases from every released version.
- Export → import round-trip; each third-party importer against sample files in `tests/fixtures/`.
- Settings: schema validation, unknown keys preserved, live reload.
- Single instance: argument forwarding and stale-socket recovery.

# OwlSql — Architecture (draft)

## Layers

```
┌────────────────────────────────────────────────────────────┐
│ QML UI — module OwlSql.Ui                                   │
│   Theme tokens · OwlSql.Style (custom Controls style)       │
│   Shell · ObjectTree · DataGrid · SqlEditor · Dialogs       │
├────────────────────────────────────────────────────────────┤
│ App layer (C++, QML_ELEMENT / QML_SINGLETON types)          │
│   SessionStore · Workspace/Tabs · ResultModel · History     │
│   CommandRegistry (palette) · Settings · Importers          │
├────────────────────────────────────────────────────────────┤
│ OwlCore (C++20, QtCore only, no GUI — unit-tested)          │
│   Driver API · Connection · async Query · ResultStream      │
│   Catalog (introspection) · Dialect (quoting, types, DDL)   │
│   SqlTokenizer · ChangeSet (staged edits → SQL)             │
│   Transport  ── local (in-process) | remote (Gateway, later)│
├──────────────┬──────────────┬──────────────┬───────────────┤
│ postgres     │ mysql        │ sqlite       │ (later) mssql, │
│ libpq async  │ MariaDB C    │ sqlite3      │ duckdb, odbc…  │
└──────────────┴──────────────┴──────────────┴───────────────┘
              driver plugins (QPluginLoader, internal ABI)
```

## Key principles

1. **No QtSql.** Native client libraries behind our own Driver API: real async I/O, cancellation,
   rich type info, server messages/notices.
2. **One worker thread per connection.** UI never blocks. Results stream to the UI in chunks
   (default fetch page 1000 rows, "load more" / "fetch all").
3. **Catalog + Dialect per driver.** The UI is generic: it asks the catalog for object
   categories/children and the dialect for quoting, type lists and DDL generation.
4. **Every change is a ChangeSet.** Grid edits and editor dialogs produce a ChangeSet → dialect
   renders SQL → DDL/DML preview → apply (in a transaction where the engine allows it).
5. **Transport abstraction from day one.** `OwlCore` exposes connections via an interface that can be
   implemented in-process (desktop) or over WebSocket to an **OwlSql Gateway** (future WASM build).
   Requests/results must therefore be serializable value types.
6. **Swappable editor.** `SqlEditor` QML component wraps `TextArea` + KSyntaxHighlighting
   (via `QQuickTextDocument`), custom gutter and completion popup, behind a clear interface so a
   scene-graph editor can replace it later.
7. **Context-aware completion** using a tokenizer (FROM/JOIN/aliases/CTEs), not a full parser, in v1.

## Storage

| What | Where |
|---|---|
| Passwords, SSH passphrases | OS keychain via QtKeychain (libsecret/KWallet, Credential Manager, Keychain); fall back to prompting |
| Sessions, folders, settings, history, workspace state | Local SQLite file in the app data directory |

## Connections

TCP, Unix sockets / named pipes, SSL/TLS (CA, client certs, verify modes), SSH tunnel (password,
key, agent; jump hosts later via libssh2), connection URL parsing, `.pgpass` / `.my.cnf`.

## Tooling

- Qt 6.12, C++20, CMake + committed `CMakePresets.json` (Qt Creator primary IDE; `.user` files ignored).
- Dependencies via **vcpkg manifest** (`vcpkg.json`): libpq, libmariadb, sqlite3, KSyntaxHighlighting,
  QtKeychain, libssh2, (crash reporting lib TBD).
- Tests: Qt Test for OwlCore; integration tests against PostgreSQL & MariaDB in Docker on CI;
  a few Qt Quick Test suites.
- GitHub + Actions: Linux/Windows/macOS matrix; release artifacts (.deb, .rpm, snap; signed Win/macOS builds).

## Proposed repo layout

```
owlsql/
├── CMakeLists.txt  CMakePresets.json  vcpkg.json
├── src/
│   ├── core/            # OwlCore library
│   ├── drivers/         # postgres/, mysql/, sqlite/
│   ├── app/             # C++ app layer exposed to QML
│   ├── ui/              # QML module OwlSql.Ui
│   ├── style/           # QML module OwlSql.Style + Theme
│   └── main.cpp
├── resources/           # icons (Tabler), fonts, logo
├── tests/               # unit/, integration/, qml/
├── tools/               # gateway/ (later), scripts
├── packaging/           # deb, rpm, snap, windows, macos
└── docs/
```

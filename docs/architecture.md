# OwlSql — Architecture

_Last reviewed: 2026-11-26, after M0. Sections marked **(planned)** do not exist in code yet._

## Layers

```
┌────────────────────────────────────────────────────────────┐
│ OwlSql.Ui     shell · gallery · (planned) tree, grid, editor│
│ OwlSql.Style  custom Qt Quick Controls style (Templates)    │
│ OwlSql.Theme  Theme tokens · fonts · icons (C++ singleton)  │
├────────────────────────────────────────────────────────────┤
│ OwlSql.App    C++ app layer exposed to QML                  │
│   done: Commands · Settings · UiState · AppDatabase ·        │
│         Keybindings · SingleInstance · AppContext           │
│   planned: SessionStore · Workspace · ResultModel · History │
├────────────────────────────────────────────────────────────┤
│ OwlCore (C++20, QtCore only, no GUI — unit-tested)          │
│   Driver API · Connection · async Query · ResultStream      │
│   Catalog (introspection) · Dialect (quoting, types, DDL)   │
│   SqlTokenizer · ChangeSet (staged edits → SQL)             │
│   Transport  ── local (in-process) | remote (Gateway, later)│
├──────────────┬──────────────┬──────────────┬───────────────┤
│ postgres     │ mysql        │ sqlite       │ (later) mssql, │
│ libpq        │ MariaDB C    │ sqlite3      │ duckdb, odbc…  │
└──────────────┴──────────────┴──────────────┴───────────────┘
              driver plugins (QPluginLoader, internal ABI)
```

## Key principles

1. **No QtSql for user databases.** Native client libraries behind our own Driver API (see
   `specs/driver-api.md`). Exception: OwlSql's own local store (`owlsql.db`) uses QtSql's bundled
   SQLite (decision 118). Driver API benefits: cancellation,
   rich type info, server messages/notices.
2. **Blocking drivers, one worker thread per connection.** UI never blocks. Results stream to the UI in chunks
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
| UI state (done), sessions, folders, snippets, workspace (planned) | `owlsql.db` — see `specs/app-store.md` |
| Query history (planned) | `history.db` |
| Preferences, shortcut overrides (done) | `settings.json`, `keybindings.json` |

## Connections

TCP, Unix sockets / named pipes, SSL/TLS (CA, client certs, verify modes), SSH tunnel (password,
key, agent; jump hosts later via libssh2), connection URL parsing, `.pgpass` / `.my.cnf`.

## Tooling

- Qt 6.12, C++20, CMake + committed `CMakePresets.json` (Qt Creator primary IDE; `.user` files ignored).
- Qt from the Qt installer locally / `install-qt-action` in CI (Qt is **not** taken from vcpkg).
- C libraries via **vcpkg manifest** (`vcpkg.json`; used automatically when `VCPKG_ROOT` is set):
  libpq, sqlite3 (M1), libmariadb (M2), libssh2 (M3). Empty after M0.
- Qt-based libraries — QtKeychain, KSyntaxHighlighting (M1) — from pinned source tags via CMake
  `FetchContent`, built against our Qt (decision 124). Crash reporting: decided before 1.0.
- Tests (done): Qt Test unit suites + an offscreen UI smoke test that fails on QML warnings and can
  save screenshots (`OWL_SCREENSHOT_DIR`). Planned: integration tests against real servers
  (PostgreSQL in M1, MariaDB/MySQL in M2) in Docker on the Linux CI job only (decision 125).
- GitHub Actions (done): build + test on Linux/GCC, Windows/MSVC, macOS; format (clang-format 21,
  qmlformat), clang-tidy, qmllint. Planned: packaging and release artifacts (M4).

## Repo layout

```
owlsql/
├── CMakeLists.txt  CMakePresets.json  vcpkg.json
├── .github/workflows/ci.yml
├── src/
│   ├── main.cpp         # executable: CLI options, single instance, AppContext, QML engine
│   ├── core/            # OwlCore (QtCore only) — version now; Driver API in M1
│   ├── theme/           # QML module OwlSql.Theme (Theme singleton, icon provider, OwlIcon)
│   ├── style/           # QML module OwlSql.Style (controls on QtQuick.Templates)
│   ├── app/             # QML module OwlSql.App (+ migrations/owlsql/*.sql)
│   ├── ui/              # QML module OwlSql.Ui (shell, gallery)
│   └── drivers/         # (planned, M1) postgres/, sqlite/; mysql/ in M2
├── resources/           # themes/*.json, fonts (Inter, JetBrains Mono), icons (Tabler), logo
├── i18n/                # owlsql_<lang>.ts (lupdate)
├── tests/               # unit/ (done); integration/ (planned)
├── tools/               # format.sh, check-format.sh, setup-dev.sh, git-hooks/
├── packaging/           # (planned, M4) deb, rpm, snap, windows, macos
└── docs/
```

Module dependencies: `Ui → Style → Theme`, `Ui → App → Theme`, `App → Core` (planned); drivers
depend only on Core. QML modules are static libraries with static plugins imported in `main.cpp`.

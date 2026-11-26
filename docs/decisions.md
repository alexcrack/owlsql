# Decision log

Accepted during the planning session on 2026-11-25. New decisions get the next number;
superseded ones are marked, never deleted.

| # | Decision | Notes |
|---|---|---|
| 001 | Public open-source product, **GPL-3.0** | Same license family as HeidiSQL |
| 002 | Linux, Windows, macOS from day one; **WASM later** | Free Linux binaries; paid Win/macOS binaries |
| 003 | Paid builds sold via store/reseller | No in-app license checks |
| 004 | Tier-1 drivers: PostgreSQL, MySQL/MariaDB, SQLite | Tier 2 and ODBC after 1.0 |
| 005 | **Qt 6.12, C++20, CMake presets, Qt Creator** | |
| 006 | Dependencies via **vcpkg manifest** | |
| 007 | Testing: core unit + DB integration in Docker + some QML tests | |
| 008 | Native driver libraries, **not QtSql** | Async, cancellable, rich types |
| 009 | Transport abstraction; future **OwlSql Gateway** for WASM | Serializable requests/results |
| 010 | Modern IDE-like visual style, fixed panes, single connection tree | Docking: maybe later |
| 011 | UX: command palette, session restore, keyboard-first grid | |
| 012 | Connections: SSH, SSL/TLS, sockets/pipes, URL import | |
| 013 | Secrets in **OS keychain** (QtKeychain) | |
| 014 | Auto-commit default + manual toggle + open-TX indicator | |
| 015 | Per-session safety level (dev/staging/prod) | |
| 016 | Editor: **TextArea + KSyntaxHighlighting**, swappable | |
| 017 | Context-aware completion via tokenizer | Full parser later if needed |
| 018 | HeidiSQL-style run keys | Ctrl+Enter / F9 / Shift+F9 |
| 019 | Paged fetch, default limit 1000, load more / fetch all | |
| 020 | v1 must-haves: table structure editor, export/import | EXPLAIN, dashboard post-1.0 |
| 021 | v1 object editors: views/matviews, routines/triggers, users/roles/grants, sequences/types/extensions | |
| 022 | JSON editor with code / constructor switch | |
| 023 | Driver plugins internal now, public SDK after 1.0 | |
| 024 | AI features later, opt-in, BYO key | |
| 025 | Code lives in `~/_WORK/PROJECTS/owlsql`; old attempts are reference only | |
| 026 | GitHub + Actions | |
| 027 | English, i18n-ready | |
| 028 | Linux packaging: .deb, .rpm, Snap | |
| 029 | Tabler Icons; bundled Inter + JetBrains Mono | |
| 030 | Built-in themes (Light/Dark/System); stable tokens for theme files later | |
| 031 | Importers: HeidiSQL, pgAdmin 4, DBeaver, MySQL Workbench | |
| 032 | Planning docs in `docs/` (vision, architecture, decisions, roadmap) | |
| 033 | Workflow: small PR per feature, conventional commits, **no AI co-author trailers** | |
| 034 | Grid filter: WHERE box + per-column filters writing into it | |
| 035 | FK navigation: jump to referenced row + lookup picker | |
| 036 | Object tree: pgAdmin depth, filterable | |
| 037 | Persistent searchable history, 90-day default retention, prod opt-out | |
| 038 | Opt-in crash reports only, no usage telemetry | |
| 039 | Update check via GitHub Releases; auto-updater for Win/macOS builds | |
| 040 | Placeholder owl logo now | |
| 041 | No deadline — quality first | |
| 042 | Drivers are **blocking**, one worker thread per physical connection; cancel is the only cross-thread call | Supersedes the "libpq async API" idea in the first proposal |
| 043 | Connections: `meta` + one per query tab (lazy) + one per browsed PG database; "single connection" session option | |
| 044 | Async facade: `QueryJob` QObjects with signals | |
| 045 | Ad-hoc "load more" keeps the result stream open (paused) on the tab's connection | |
| 046 | Own `Value` type (lossless, keeps native type, CBOR-encodable) | |
| 047 | Large values truncated at 64 KB (configurable), full value fetched by PK on open | |
| 048 | Minimum servers: PostgreSQL 13, MySQL 5.7, MariaDB 10.6, SQLite 3.35 | |
| 049 | Drivers are shared plugins (QPluginLoader), internal ABI | |
| 050 | Catalog cache: lazy + targeted invalidation + manual F5, no polling | |
| 051 | Reconnect: silent when idle (retry reads once), never retry writes, report rolled-back transactions | |
| 052 | Placeholders → prompt dialog → real bound parameters | |
| 053 | Log panel shows all statements (user, internal, browse, notices), filterable | |
| 054 | Accent: owl amber (`#E8A33D` dark / `#C7801A` light line) | |
| 055 | Warm neutral greys | |
| 056 | Native title bar; macOS global menu bar | Custom unified title bar not planned |
| 057 | Subtle motion (100–150 ms), honour reduced motion | |
| 058 | Compact density: Inter 13 px, 24 px rows, 28 px controls | |
| 059 | Grid: dimmed italic NULL by default; per-type value colours as a toggle; zebra + vertical lines configurable | |
| 060 | Radius 4 px controls, 8 px popups | |
| 061 | Underline-style workspace tabs | |
| 062 | Own Owl Light/Dark editor highlighting themes; other KSyntaxHighlighting themes selectable | |
| 063 | Safety levels: session colour tag everywhere; prod adds red workspace frame + PROD badge | |
| 064 | Accessibility target: basic (keyboard reachable, focus visible) | |
| 065 | Session manager lives in a Welcome tab | |
| 066 | Object tab with inner tabs (Data, Structure, Indexes, FKs, DDL, Stats) | |
| 067 | Status bar: session + level, transaction state, query metrics, cursor/selection info | |
| 068 | Bottom panel collapsed by default, auto-opens on errors/notices, Ctrl+J | |
| 069 | Theme tokens in JSON files loaded by a C++ `ThemeManager` QML singleton; dev-only Component Gallery | |
| 070 | Crash reporting library: decide before 1.0; only the opt-in hook now | |
| 071 | Snap: strict confinement + network, ssh-keys, password-manager-service, personal-files, home; TCP fallback for local sockets | |
| 072 | Portable mode: later; keep data paths abstracted | |
| 073 | Minimum OS = Qt 6.12's supported platforms; Linux packages for Ubuntu 24.04+, Debian 12+, Fedora 41+, RHEL/Rocky 9+ | |
| 074 | Auto-updater: Sparkle (macOS) + WinSparkle (Windows) | |
| 075 | Linux .deb/.rpm bundle a private Qt under /opt/owlsql | |
| 076 | Code style: Qt-like clang-format, clang-tidy, qmllint/qmlformat — enforced in CI + pre-commit | |
| 077 | Windows installer: MSI via WiX | macOS: notarized DMG |
| 078 | `Result<T>` = `tl::expected` on C++20 | |
| 079 | CBOR encoding on value types from M1 (no transport yet) | |
| 080 | LISTEN/NOTIFY: post-1.0 entirely | |
| 081 | Shortcuts: platform-native (QKeySequence standard keys), all actions remappable via the command registry | Registry from M0, remap UI after M3 |
| 082 | Single main window; dialogs/editors as popups | |
| 083 | App data in `owlsql.db` (sessions, workspace, snippets) + separate `history.db` (FTS5) | |
| 084 | Preferences in human-editable `settings.json` (non-defaults only, schema-driven) + `keybindings.json` | |
| 085 | Team sharing via `*.owlsql-sessions.json` export/import, never secrets; importers reuse the same pipeline | |
| 086 | Single instance via QLocalServer; second launch forwards arguments | |
| 087 | Query tabs: autosaved scratch buffers in owlsql.db, optionally linked to .sql files | |
| 088 | Session options: startup script, default db/schema + visible databases, query defaults, notes | |
| 089 | Forward-only embedded migrations, `PRAGMA user_version`, automatic backup (keep 3) | |
| 090 | Restore tabs immediately, connect lazily; prod never auto-connects | |
| 091 | Own token-based SQL formatter (keyword case, clause newlines, indents) | |
| 092 | Current statement via dialect splitter, visibly tinted with a gutter ▶ marker | |
| 093 | Completion: auto after `.` and 2 chars in object/column contexts, Ctrl+Space anywhere | |
| 094 | Completion covers tables, columns, keywords, functions (with signatures), FK-based JOIN conditions | No snippets or `*` expansion in v1 |
| 095 | Editor target ~5 MB; bigger files via streaming "Run SQL file…" | |
| 096 | Diagnostics: server error positions only, no live lint in v1 | |
| 097 | Editor v1: folding, regex find/replace, bracket match + auto-close; no multi-cursor/minimap | |
| 098 | Editor modes: Sql, ReadOnlySql, Json, routine body; M1 starts with a TextArea feasibility spike | |
| 099 | Grid editing is spreadsheet-like (type to edit, Enter/Tab commit), changes staged until Apply; optional per-session "apply rows immediately" | |
| 100 | Query results editable only when columns map to a base table whose row key is in the result | |
| 101 | Row key = primary key, else unique non-null index, else read-only | No ctid/rowid/all-columns matching |
| 102 | Apply = one transaction with SQL preview; first error rolls back and keeps changes staged | |
| 103 | Columns: hide/reorder/resize (persisted), multi-column sort, record view; no frozen columns in v1 | |
| 104 | Cell editors: date/time pickers, enum/set/bool, BLOB hex/image/file, large text popup, JSON editor, FK lookup | |
| 105 | Grid ops: Copy as (TSV/CSV/JSON/Markdown/INSERT/IN), paste from spreadsheets, bulk set, insert/duplicate rows | |
| 106 | Table browsing uses pages of 1000 with a footer pager; query results stream with load more | |
| 107 | `OwlSql.Theme` module: C++ `Theme` singleton with typed token groups loaded from JSON; System mode follows `QStyleHints::colorScheme`, explicit modes are passed to the platform | |
| 108 | Reduced motion is an app setting (`Theme.reduceMotion`); Qt exposes no OS reduced-motion hint | |
| 109 | Icons via `image://owlicon/<name>?color=…` provider (Tabler SVG tinted at render time); bundled Tabler 3.50.0, Inter 4.1, JetBrains Mono 2.304 | |
| 110 | `OwlSql.Style`: app QML imports it directly (compile-time style selection) and it is also the runtime style, so attached ToolTips and Qt's QML dialogs match; Basic is the fallback | |
| 111 | Button API: `variant` (primary/secondary/ghost/danger), `small`, `icon.name` = Tabler icon; disabled controls fade via opacity | |
| 112 | Popup shadows are drawn behind the surface (MultiEffect), never as a layer effect, so surfaces survive the software renderer | |
| 113 | SpinBox shows plain integers (no locale digit grouping) — ports, limits, row counts | |
| 114 | `OwlSql.App` QML module = C++ app layer; executable entry point is `src/main.cpp` | Matches architecture.md layout |
| 115 | Command registry: stable ids `area.action`; menus and shortcuts use `CommandAction`; QML binds to `Commands.titles` / `shortcuts` / `shortcutTexts` maps so keybinding overrides apply live | |
| 116 | Close Tab = Ctrl+W (⌘W), not `QKeySequence::Close` (Ctrl+F4 on Linux) | |
| 117 | Style strips `&` mnemonic markers when drawing text (Alt-key underlines not shown yet) | |
| 118 | App store (`owlsql.db`, later `history.db`) uses QtSql's bundled SQLite driver — local app storage, not a user-database driver (decision 008 unaffected) | |
| 119 | `owlsql.db` v1 contains only `ui_state`; other tables arrive as migrations with their features. Migration files: one statement per `;` at line end | |
| 120 | `AppContext` owns settings, UI state, keybindings and the app database; QML singletons resolve through it; `--data-dir` isolates everything | |
| 121 | Theme choice is a setting (`theme.mode`); theme commands write settings, `AppContext` applies them to `Theme` | |
| 122 | Window size/maximized state, panel visibility, splitter sizes and open tabs are restored; window position is not (multi-monitor safety) | |
| 123 | i18n: `qt_add_translations` with English source; `i18n/owlsql_en.ts` (plurals only); `.qm` embedded under `:/i18n`, loaded for the system locale | |
| 124 | vcpkg only for C libraries (libpq, sqlite3, libmariadb, libssh2); Qt-based libraries (QtKeychain, KSyntaxHighlighting) built from pinned tags via CMake FetchContent against our Qt | Amends 006: vcpkg has no KSyntaxHighlighting port and `qtkeychain-qt6` pulls vcpkg's own qtbase |
| 125 | Server integration tests on the Linux CI job only (Docker service containers); SQLite + unit tests on all OSes | macOS runners have no Docker |
| 126 | Qt-based dependencies are downloaded with `FetchContent` and built + installed at configure time in a separate CMake process (`cmake/OwlExternal.cmake`), static, reused via a stamp file; CI caches the install prefix | Building KDE frameworks as subprojects would leak their global settings (compiler flags, git hooks, install rules) into ours |
| 127 | vcpkg pinned by `builtin-baseline` (13eb3d1, 2026-11-09); CI uses its own vcpkg checkout at that commit (overriding Visual Studio's `VCPKG_ROOT`); Windows triplet `x64-windows-static-md` | |
| 128 | Linux keychain: libsecret required in CI and release builds (`OWL_REQUIRE_LIBSECRET`, set by the `ci` preset); local builds without libsecret fall back to KWallet with a warning | |
| 129 | SQL editor v1 stays on QML `TextArea`; smooth editing target **2 MB** (was 5 MB); 2–5 MB opens with a "large script" banner; > 5 MB only via Run SQL file… | Supersedes 095. M1.2: typing 10 ms at 1 MB, 16 ms at 2 MB, ~40 ms at 5 MB inside `QQuickTextEdit` |
| 130 | Own `EditorHighlighter` on `KSyntaxHighlighting::AbstractHighlighter` with batched `QTextLayout::setFormats` and one dirty mark per pass — not `QSyntaxHighlighter` | M1.2: `QSyntaxHighlighter` full re-highlight of 1 MB took ~80 s; batched 0.52 s |
| 131 | Code folding via `QTextBlock::setVisible(false)` + one `markContentsDirty` — works in `QQuickTextEdit` | M1.2 |
| 132 | Editor experiments live on `spike/*` branches and are never merged; findings are recorded in the specs and this log | |
| 133 | `owl::Value` = `std::variant` with one alternative per kind (index identifies the kind); temporal/interval/array payloads shared and immutable; 40 bytes, enforced by a test | Grid memory budget (`specs/data-grid.md` §11) |
| 134 | CBOR wire format v1 for `Value`, `RowBatch`, `ColumnInfo`, `DbError`, `ServerMessage` as in `specs/driver-api.md` §2.1; enum numbers are wire codes (append only) | |
| 135 | Display formatting of values lives in the app layer (grid, M1.10), not in `owl::Value` | Keeps OwlCore free of UI/locale concerns |
| 136 | `CMAKE_CXX_SCAN_FOR_MODULES OFF`: no C++20 modules; avoids per-file scanning and module flags clang-tidy rejects (GCC 14+) | |

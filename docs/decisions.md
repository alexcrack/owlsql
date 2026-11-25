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

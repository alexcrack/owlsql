-- SPDX-License-Identifier: GPL-3.0-or-later
-- owlsql.db v1 (docs/specs/app-store.md §3). One statement per ';' at end of line.
-- Later tables (folder, session, snippet, buffer, workspace_tab) arrive with the features that
-- use them, as new migrations.

CREATE TABLE ui_state (
  key   TEXT PRIMARY KEY,
  value TEXT NOT NULL
);

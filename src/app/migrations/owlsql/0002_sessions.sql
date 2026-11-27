-- SPDX-License-Identifier: GPL-3.0-or-later
-- owlsql.db v2: session folders and sessions (docs/specs/app-store.md §3, M1.9a).
-- Secrets are never stored here: they live in the OS keychain (§2).

CREATE TABLE folder (
  id          TEXT PRIMARY KEY,
  parent_id   TEXT REFERENCES folder(id) ON DELETE CASCADE,
  name        TEXT NOT NULL,
  position    INTEGER NOT NULL DEFAULT 0,
  expanded    INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE session (
  id             TEXT PRIMARY KEY,
  folder_id      TEXT REFERENCES folder(id) ON DELETE SET NULL,
  position       INTEGER NOT NULL DEFAULT 0,
  name           TEXT NOT NULL,
  driver_id      TEXT NOT NULL,
  params         TEXT NOT NULL,
  ssh            TEXT,
  safety_level   TEXT NOT NULL DEFAULT 'dev',
  color          TEXT,
  options        TEXT NOT NULL DEFAULT '{}',
  notes          TEXT,
  secret_modes   TEXT NOT NULL DEFAULT '{}',
  created_at     TEXT NOT NULL,
  updated_at     TEXT NOT NULL,
  last_used_at   TEXT,
  imported_from  TEXT
);

CREATE INDEX session_folder ON session(folder_id, position);

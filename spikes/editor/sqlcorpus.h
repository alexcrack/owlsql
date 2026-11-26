// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>

// Realistic PostgreSQL script of roughly `bytes` size: DDL, INSERT batches, queries with joins
// and CTEs, comments, and PL/pgSQL functions with $$ bodies.
QString makeSqlCorpus(qsizetype bytes);

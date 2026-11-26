// SPDX-License-Identifier: GPL-3.0-or-later
#include "sqlcorpus.h"

QString makeSqlCorpus(qsizetype bytes)
{
    QString out;
    out.reserve(bytes + 4096);
    for (int n = 0; out.size() < bytes; ++n) {
        out += QStringLiteral("-- section %1: users and orders\n"
                              "CREATE TABLE IF NOT EXISTS app.users_%1 (\n"
                              "    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,\n"
                              "    email text NOT NULL UNIQUE,\n"
                              "    meta jsonb NOT NULL DEFAULT '{}',\n"
                              "    created_at timestamptz NOT NULL DEFAULT now()\n"
                              ");\n\n")
                   .arg(n);
        out += QStringLiteral("INSERT INTO app.users_%1 (email, meta) VALUES\n").arg(n);
        for (int r = 0; r < 20; ++r)
            out += QStringLiteral(
                       "    ('user%1_%2@example.com', '{\"plan\": \"pro\", \"seats\": %2}')%3\n")
                       .arg(n)
                       .arg(r)
                       .arg(r == 19 ? QStringLiteral(";") : QStringLiteral(","));
        out += QStringLiteral(
                   "\nWITH recent AS (\n"
                   "    SELECT u.id, u.email, count(o.id) AS orders\n"
                   "    FROM app.users_%1 u\n"
                   "    LEFT JOIN app.orders o ON o.user_id = u.id AND o.status <> 'cancelled'\n"
                   "    WHERE u.created_at > now() - interval '30 days' /* last month */\n"
                   "    GROUP BY 1, 2\n"
                   ")\n"
                   "SELECT * FROM recent WHERE orders > 3 ORDER BY orders DESC LIMIT 100;\n\n"
                   "CREATE OR REPLACE FUNCTION app.touch_%1() RETURNS trigger\n"
                   "LANGUAGE plpgsql AS $$\n"
                   "BEGIN\n"
                   "    NEW.updated_at := now(); -- keep audit columns fresh\n"
                   "    IF NEW.email IS DISTINCT FROM OLD.email THEN\n"
                   "        RAISE NOTICE 'email changed for %%', NEW.id;\n"
                   "    END IF;\n"
                   "    RETURN NEW;\n"
                   "END;\n"
                   "$$;\n\n")
                   .arg(n);
    }
    return out;
}

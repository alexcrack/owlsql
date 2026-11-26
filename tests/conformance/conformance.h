// SPDX-License-Identifier: GPL-3.0-or-later
// Driver conformance suite (specs/driver-api.md §12): the same checks for every driver, fed with
// engine-specific SQL. Each driver has a small test executable that fills in the fixtures and
// calls runConformance().
#pragma once

#include "owl/core/driver.h"

#include <functional>

namespace owl::conformance {

struct Fixtures
{
    QString driverId;
    QString pluginDirectory; // loaded through DriverRegistry, like the app does
    ConnectionParams params;
    // Unless empty, connecting is required; otherwise every test is skipped with this reason.
    QString skipReason;

    // One row: 42, 4611686018427387904, -7, 2.5, 'Ωmega 🦉', NULL, '', bytes 00 ff 10
    QString selectScalars;
    // n rows of one integer column with values 1..n
    std::function<QString(int)> generateRows;
    // A text value of n 'x' characters
    std::function<QString(int)> bigText;
    // Runs until cancelled
    QString runaway;
    // Three placeholders: an integer, a text and a null → returns them as one row
    QString selectThreeParams;
    // A syntax error and the expected error position (UTF-16 offset), -1 if the engine has none
    QString syntaxError;
    int syntaxErrorPosition = -1;
    // Table for constraint / affected-rows tests: one INTEGER column `x` with a UNIQUE constraint
    QString createUniqueTable; // CREATE TEMP TABLE owl_conf (x … UNIQUE)
    QString insertThreeRows; // INSERT INTO owl_conf VALUES (1), (2), (3)
    QString insertDuplicate; // INSERT INTO owl_conf VALUES (1)
    QString updateAllRows; // UPDATE owl_conf SET x = x + 10
    // A statement that writes, for the read-only guard (must not need owl_conf)
    QString writeStatement;
};

int runConformance(int argc, char **argv, const Fixtures &fixtures);

} // namespace owl::conformance

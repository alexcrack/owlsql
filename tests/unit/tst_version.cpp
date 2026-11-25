// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/version.h"

#include <QTest>

class TestVersion : public QObject
{
    Q_OBJECT

private slots:
    void versionStringMatchesComponents()
    {
        const QString expected = QStringLiteral("%1.%2.%3")
                                     .arg(owl::core::versionMajor)
                                     .arg(owl::core::versionMinor)
                                     .arg(owl::core::versionPatch);
        QCOMPARE(owl::core::versionString(), expected);
    }
};

QTEST_GUILESS_MAIN(TestVersion)
#include "tst_version.moc"

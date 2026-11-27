// SPDX-License-Identifier: GPL-3.0-or-later
// MySQL / MariaDB text values and parameter text (src/drivers/mysql/mysqlvalues.h).
#include "mysqlvalues.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace owl;
using namespace owl::mysql;

namespace {
constexpr qint64 us = 1'000'000;

Value parse(QByteArrayView text, ValueKind kind, qsizetype limit = qsizetype{64} * 1024)
{
    return parseValue(text, kind, limit);
}

QByteArray param(const Value &v)
{
    return toParamText(v).value_or("<NULL>");
}
} // namespace

class TestMySqlValues : public QObject
{
    Q_OBJECT

private slots:
    void numbers()
    {
        QCOMPARE(parse("-9223372036854775808", ValueKind::Int),
                 Value::fromInt(std::numeric_limits<qint64>::min()));
        QCOMPARE(parse("18446744073709551615", ValueKind::UInt),
                 Value::fromUInt(std::numeric_limits<quint64>::max()));
        QCOMPARE(parse("1e308", ValueKind::Double), Value::fromDouble(1e308));
        QCOMPARE(parse("-0.000000001", ValueKind::Decimal), Value::decimal(u"-0.000000001"_s));
    }

    void temporals()
    {
        QCOMPARE(parse("2024-02-29", ValueKind::Date).toTemporal().date, QDate(2024, 2, 29));
        const Temporal zero = parse("0000-00-00", ValueKind::Date).toTemporal();
        QVERIFY(!zero.date.isValid()); // MySQL zero date: text only
        QCOMPARE(zero.original, u"0000-00-00"_s);
        const Temporal dt = parse("2024-02-29 13:45:12.000001", ValueKind::Timestamp).toTemporal();
        QCOMPARE(dt.date, QDate(2024, 2, 29));
        QCOMPARE(dt.timeMicros, us * (13 * 3600 + 45 * 60 + 12) + 1);
        QVERIFY(!dt.offsetSeconds);
        const Temporal zeroDt = parse("0000-00-00 00:00:00", ValueKind::Timestamp).toTemporal();
        QVERIFY(!zeroDt.date.isValid());
        QCOMPARE(zeroDt.timeMicros, -1);
    }

    void timeIsTimeOrDuration()
    {
        const Value t = parse("23:59:59.5", ValueKind::Time);
        QCOMPARE(t.kind(), ValueKind::Time);
        QCOMPARE(t.toTemporal().timeMicros, us * (23 * 3600 + 59 * 60 + 59) + us / 2);
        const Value longer = parse("838:59:59", ValueKind::Time);
        QCOMPARE(longer.kind(), ValueKind::Interval);
        QCOMPARE(longer.toInterval().micros, us * (838 * 3600 + 59 * 60 + 59));
        const Value negative = parse("-00:00:01", ValueKind::Time);
        QCOMPARE(negative.kind(), ValueKind::Interval);
        QCOMPARE(negative.toInterval().micros, -us);
        QCOMPARE(negative.toInterval().original, u"-00:00:01"_s);
    }

    void textAndBytes()
    {
        const Value cut = parse("ΩΩ", ValueKind::Text, 3);
        QCOMPARE(cut.toText(), u"Ω"_s);
        QCOMPARE(cut.fullSize(), 4);
        const Value bytes = parse(QByteArrayView("\x00\x01\x02", 3), ValueKind::Bytes, 2);
        QCOMPARE(bytes.toBytes(), QByteArray("\x00\x01", 2));
        QCOMPARE(bytes.fullSize(), 3);
        QCOMPARE(parse("{\"a\": 1}", ValueKind::Json), Value::json(u"{\"a\": 1}"_s));
    }

    void params()
    {
        QCOMPARE(toParamText(Value()), std::nullopt);
        QCOMPARE(param(Value::date(QDate(2024, 2, 29))), QByteArray("2024-02-29"));
        QCOMPARE(param(Value::timestamp(QDate(2024, 2, 29), us * 13 * 3600 + 5)),
                 QByteArray("2024-02-29 13:00:00.000005"));
        QCOMPARE(param(Value::interval({.months = 0, .days = 1, .micros = -us, .original = {}})),
                 QByteArray("23:59:59"));
        QCOMPARE(param(Value::interval({.months = 0, .days = 0, .micros = -us, .original = {}})),
                 QByteArray("-00:00:01"));
        QCOMPARE(param(Value::fromBool(true)), QByteArray("1"));
        QCOMPARE(param(Value::text(u"Ω"_s)), QByteArray("Ω"));
    }
};

QTEST_GUILESS_MAIN(TestMySqlValues)
#include "tst_mysql_values.moc"

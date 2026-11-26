// SPDX-License-Identifier: GPL-3.0-or-later
// PostgreSQL text-format parsing and parameter formatting (src/drivers/postgres/pgvalues.h).
#include "pgvalues.h"

#include <QTest>

#include <cmath>

using namespace Qt::StringLiterals;
using namespace owl;
using namespace owl::postgres;

namespace {
constexpr qint64 us = 1'000'000;

QByteArray param(const Value &v)
{
    return toParamText(v).value_or("<NULL>");
}

Value parse(QByteArrayView text, quint32 typeOid, qsizetype limit = qsizetype{64} * 1024)
{
    static const TypeRegistry types;
    return parseValue(text, typeOid, types, limit);
}
} // namespace

class TestPgValues : public QObject
{
    Q_OBJECT

private slots:
    void scalars()
    {
        QCOMPARE(parse("t", oid::Bool), Value::fromBool(true));
        QCOMPARE(parse("f", oid::Bool), Value::fromBool(false));
        QCOMPARE(parse("-9223372036854775808", oid::Int8),
                 Value::fromInt(std::numeric_limits<qint64>::min()));
        QCOMPARE(parse("18446744073709551615", oid::Xid8),
                 Value::fromUInt(std::numeric_limits<quint64>::max()));
        QCOMPARE(parse("2.5", oid::Float8), Value::fromDouble(2.5));
        QVERIFY(std::isinf(parse("-Infinity", oid::Float4).toDouble()));
        QVERIFY(std::isnan(parse("NaN", oid::Float8).toDouble()));
        QCOMPARE(parse("12345678901234567890.000000001", oid::Numeric),
                 Value::decimal(u"12345678901234567890.000000001"_s));
        QCOMPARE(parse("Ωmega 🦉", oid::Varchar), Value::text(u"Ωmega 🦉"_s));
        QCOMPARE(parse("{\"a\": 1}", oid::Jsonb), Value::json(u"{\"a\": 1}"_s));
        QCOMPARE(parse("a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11", oid::Uuid),
                 Value::uuid(QUuid(u"a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11"_s)));
        QCOMPARE(parse("192.168.0.1/32", 869).kind(), ValueKind::Unknown); // inet
        QCOMPARE(parse("whatever", 999999), Value::unknown(u"whatever"_s));
    }

    void bytea()
    {
        QCOMPARE(parse("\\x00ff10", oid::Bytea), Value::bytes(QByteArray("\x00\xff\x10", 3)));
        // bytea_output = escape
        QCOMPARE(parse("a\\000\\\\b", oid::Bytea), Value::bytes(QByteArray("a\x00\\b", 4)));
        const Value cut = parse("\\x00010203", oid::Bytea, 2);
        QCOMPARE(cut.toBytes(), QByteArray("\x00\x01", 2));
        QCOMPARE(cut.fullSize(), 4);
    }

    void truncation()
    {
        const Value v = parse("Ωmega", oid::Text, 2); // Ω is two bytes; "m" would be the third
        QCOMPARE(v.toText(), u"Ω"_s);
        QVERIFY(v.isTruncated());
        QCOMPARE(v.fullSize(), 6);
        const Value inside = parse("ΩΩ", oid::Text, 3); // never splits a character
        QCOMPARE(inside.toText(), u"Ω"_s);
    }

    void dates()
    {
        QCOMPARE(parse("2024-02-29", oid::Date).toTemporal().date, QDate(2024, 2, 29));
        QCOMPARE(parse("0044-03-15 BC", oid::Date).toTemporal().date, QDate(-44, 3, 15));
        QCOMPARE(parse("12345-01-01", oid::Date).toTemporal().date, QDate(12345, 1, 1));
        const Temporal inf = parse("-infinity", oid::Date).toTemporal();
        QCOMPARE(inf.special, Temporal::Special::MinusInfinity);
        QCOMPARE(inf.original, u"-infinity"_s);
        // A DateStyle we did not set: the text survives, the parts stay invalid.
        const Temporal us_style = parse("02/29/2024", oid::Date).toTemporal();
        QVERIFY(!us_style.date.isValid());
        QCOMPARE(us_style.original, u"02/29/2024"_s);
    }

    void times()
    {
        const Temporal t = parse("13:45:12.123456", oid::Time).toTemporal();
        QCOMPARE(t.timeMicros, (13 * 3600 + 45 * 60 + 12) * us + 123456);
        QCOMPARE(parse("24:00:00", oid::Time).toTemporal().timeMicros, us * 24 * 3600);
        QCOMPARE(parse("00:00:00.5", oid::Time).toTemporal().timeMicros, us / 2);
        const Temporal tz = parse("13:45:12-05:30", oid::TimeTz).toTemporal();
        QCOMPARE(tz.offsetSeconds, -(5 * 3600 + 30 * 60));
        QCOMPARE(parse("01:02:03+02:03:04", oid::TimeTz).toTemporal().offsetSeconds,
                 2 * 3600 + 3 * 60 + 4);
    }

    void timestamps()
    {
        const Temporal ts = parse("2024-02-29 13:45:12.5", oid::Timestamp).toTemporal();
        QCOMPARE(ts.date, QDate(2024, 2, 29));
        QCOMPARE(ts.timeMicros, (13 * 3600 + 45 * 60 + 12) * us + us / 2);
        QVERIFY(!ts.offsetSeconds);
        const Temporal tz = parse("2024-02-29 13:45:12+01", oid::TimestampTz).toTemporal();
        QCOMPARE(tz.offsetSeconds, 3600);
        const Temporal bc = parse("0044-03-15 12:00:00+00:53:28 BC", oid::TimestampTz).toTemporal();
        QCOMPARE(bc.date, QDate(-44, 3, 15));
        QCOMPARE(bc.offsetSeconds, 53 * 60 + 28);
        QCOMPARE(parse("infinity", oid::TimestampTz).toTemporal().special,
                 Temporal::Special::PlusInfinity);
    }

    void intervals()
    {
        const Interval a = parse("1 year 2 mons -3 days +04:05:06.5", oid::Interval).toInterval();
        QCOMPARE(a.months, 14);
        QCOMPARE(a.days, -3);
        QCOMPARE(a.micros, (4 * 3600 + 5 * 60 + 6) * us + us / 2);
        QCOMPARE(a.original, u"1 year 2 mons -3 days +04:05:06.5"_s);
        QCOMPARE(parse("-00:00:01", oid::Interval).toInterval().micros, -us);
        QCOMPARE(parse("1 day", oid::Interval).toInterval().days, 1);
        QCOMPARE(parse("2562047788:00:54.775807", oid::Interval).toInterval().micros,
                 std::numeric_limits<qint64>::max());
        // IntervalStyle iso_8601: kept as text only
        const Interval iso = parse("P1Y2M", oid::Interval).toInterval();
        QCOMPARE(iso.months, 0);
        QCOMPARE(iso.original, u"P1Y2M"_s);
    }

    void arrays()
    {
        QCOMPARE(parse("{1,NULL,3}", 1007),
                 Value::array({Value::fromInt(1), Value(), Value::fromInt(3)}));
        QCOMPARE(parse("{\"x y\",\"q\\\"uote\",\"NULL\",NULL}", 1009),
                 Value::array({Value::text(u"x y"_s), Value::text(u"q\"uote"_s),
                               Value::text(u"NULL"_s), Value()}));
        QCOMPARE(parse("{{1,2},{3,4}}", 1007),
                 Value::array({Value::array({Value::fromInt(1), Value::fromInt(2)}),
                               Value::array({Value::fromInt(3), Value::fromInt(4)})}));
        QCOMPARE(parse("[0:1]={7,8}", 1007), Value::array({Value::fromInt(7), Value::fromInt(8)}));
        QCOMPARE(parse("{}", 1009), Value::array({}));
        QCOMPARE(parse("{2024-01-01,infinity}", 1182).toArray().at(1).toTemporal().special,
                 Temporal::Special::PlusInfinity);
        QCOMPARE(parse("{(1,1),(0,0);(2,2),(1,1)}", 1020).kind(), ValueKind::Unknown); // box[]
        QCOMPARE(parse("{1,2", 1007).kind(), ValueKind::Unknown); // malformed
    }

    void typeNames()
    {
        const TypeRegistry types;
        QCOMPARE(types.displayName(oid::Numeric, ((10 << 16) | 2) + 4), u"numeric(10,2)"_s);
        QCOMPARE(types.displayName(oid::Varchar, 255 + 4), u"varchar(255)"_s);
        QCOMPARE(types.displayName(oid::TimestampTz, 3), u"timestamptz(3)"_s);
        QCOMPARE(types.displayName(1007, -1), u"int4[]"_s);
        QCOMPARE(types.displayName(oid::Text, -1), u"text"_s);
        QCOMPARE(types.displayName(424242, -1), QString());
        const TypeModifiers m = decodeTypmod(oid::Numeric, ((5 << 16) | (0x7ff & -2)) + 4);
        QCOMPARE(m.precision, 5);
        QCOMPARE(m.scale, -2); // PostgreSQL 15+: numeric(5,-2)
    }

    void params()
    {
        QCOMPARE(toParamText(Value()), std::nullopt);
        QCOMPARE(param(Value::fromBool(true)), QByteArray("t"));
        QCOMPARE(param(Value::fromDouble(-std::numeric_limits<double>::infinity())),
                 QByteArray("-Infinity"));
        QCOMPARE(param(Value::bytes(QByteArray("\x00\xff", 2))), QByteArray("\\x00ff"));
        QCOMPARE(param(Value::date(QDate(-44, 3, 15))), QByteArray("0044-03-15 BC"));
        QCOMPARE(param(Value::timestampTz(QDate(2024, 2, 29), us * 13 * 3600 + 5, -16200)),
                 QByteArray("2024-02-29 13:00:00.000005-04:30"));
        QCOMPARE(param(Value::date(QDate(), u"infinity"_s)), QByteArray("infinity"));
        QCOMPARE(param(Value::interval({.months = 14, .days = -3, .micros = -us, .original = {}})),
                 QByteArray("14 mons -3 days -00:00:01"));
        QCOMPARE(param(Value::array(
                     {Value::text(u"a\"b"_s), Value(), Value::array({Value::fromInt(1)})})),
                 QByteArray("{\"a\\\"b\",NULL,{\"1\"}}"));
        QCOMPARE(param(Value::text(u"Ω"_s)), QByteArray("Ω"));
    }
};

QTEST_GUILESS_MAIN(TestPgValues)
#include "tst_pg_values.moc"

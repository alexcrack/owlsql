// SPDX-License-Identifier: GPL-3.0-or-later
// owl::Value, ColumnInfo, RowBatch, DbError, ServerMessage, Result<T> (M1.3).
#include "owl/core/dberror.h"
#include "owl/core/result.h"
#include "owl/core/resultset.h"
#include "owl/core/value.h"

#include <QCborArray>
#include <QCborMap>
#include <QTest>

#include <cmath>
#include <limits>

using namespace Qt::StringLiterals;
using owl::Value;
using owl::ValueKind;

Q_DECLARE_METATYPE(owl::Value)

namespace {

// Through serialized bytes, the way the Gateway will carry values.
template<typename T>
std::optional<T> cborRoundTrip(const T &value)
{
    const QByteArray bytes = value.toCbor().toCbor();
    return T::fromCbor(QCborValue::fromCbor(bytes));
}

constexpr qint64 micros(int h, int m, int s, int us)
{
    return ((static_cast<qint64>(h) * 60 + m) * 60 + s) * 1'000'000 + us;
}

} // namespace

class TestCoreTypes : public QObject
{
    Q_OBJECT

private slots:
    void valueStaysSmall()
    {
        // The grid keeps millions of cells (specs/data-grid.md §11).
        qInfo("sizeof(owl::Value) = %zu bytes", sizeof(Value));
        QVERIFY(sizeof(Value) <= 40);
    }

    void roundTrip_data()
    {
        QTest::addColumn<Value>("value");
        QTest::addColumn<ValueKind>("kind");
        constexpr qint64 i64max = std::numeric_limits<qint64>::max();
        constexpr qint64 i64min = std::numeric_limits<qint64>::min();
        constexpr quint64 u64max = std::numeric_limits<quint64>::max();
        constexpr double inf = std::numeric_limits<double>::infinity();

        QTest::newRow("null") << Value() << ValueKind::Null;
        QTest::newRow("bool true") << Value::fromBool(true) << ValueKind::Bool;
        QTest::newRow("bool false") << Value::fromBool(false) << ValueKind::Bool;
        QTest::newRow("int 0") << Value::fromInt(0) << ValueKind::Int;
        QTest::newRow("int max") << Value::fromInt(i64max) << ValueKind::Int;
        QTest::newRow("int min") << Value::fromInt(i64min) << ValueKind::Int;
        QTest::newRow("uint 0") << Value::fromUInt(0) << ValueKind::UInt;
        QTest::newRow("uint > int64 max")
            << Value::fromUInt(static_cast<quint64>(i64max) + 1) << ValueKind::UInt;
        QTest::newRow("uint max") << Value::fromUInt(u64max) << ValueKind::UInt;
        QTest::newRow("double") << Value::fromDouble(1234.5678) << ValueKind::Double;
        QTest::newRow("double -0") << Value::fromDouble(-0.0) << ValueKind::Double;
        QTest::newRow("double max")
            << Value::fromDouble(std::numeric_limits<double>::max()) << ValueKind::Double;
        QTest::newRow("double denormal")
            << Value::fromDouble(std::numeric_limits<double>::denorm_min()) << ValueKind::Double;
        QTest::newRow("double NaN") << Value::fromDouble(std::nan("")) << ValueKind::Double;
        QTest::newRow("double +inf") << Value::fromDouble(inf) << ValueKind::Double;
        QTest::newRow("double -inf") << Value::fromDouble(-inf) << ValueKind::Double;
        QTest::newRow("decimal exact")
            << Value::decimal(u"12345678901234567890.000000000000000001"_s) << ValueKind::Decimal;
        QTest::newRow("decimal NaN") << Value::decimal(u"NaN"_s) << ValueKind::Decimal;
        QTest::newRow("text empty") << Value::text(QString()) << ValueKind::Text;
        QTest::newRow("text unicode") << Value::text(u"Ωmega 🦉 ✓ ñ"_s) << ValueKind::Text;
        QTest::newRow("text with NUL") << Value::text(QString(u"a\0b"_s)) << ValueKind::Text;
        QTest::newRow("bytes empty") << Value::bytes({}) << ValueKind::Bytes;
        QTest::newRow("bytes binary")
            << Value::bytes(QByteArray("\x00\x01\xff\x00", 4)) << ValueKind::Bytes;
        QTest::newRow("bytes 64 KB")
            << Value::bytes(QByteArray(qsizetype{64} * 1024, '\x7f')) << ValueKind::Bytes;
        QTest::newRow("uuid") << Value::uuid(QUuid(u"{67c8770b-44f1-410a-ab9a-f9b5446f13ee}"_s))
                              << ValueKind::Uuid;
        QTest::newRow("json") << Value::json(uR"({"a":[1,2,{"b":null}]})"_s) << ValueKind::Json;
        QTest::newRow("unknown (box)") << Value::unknown(u"(3,4),(1,2)"_s) << ValueKind::Unknown;

        QTest::newRow("date") << Value::date(QDate(2026, 11, 26), u"2026-11-26"_s)
                              << ValueKind::Date;
        QTest::newRow("date BC") << Value::date(QDate(-44, 3, 15), u"0044-03-15 BC"_s)
                                 << ValueKind::Date;
        owl::Temporal zero;
        zero.original = u"0000-00-00"_s; // MySQL zero date: no valid QDate, original kept
        QTest::newRow("date zero (MySQL)")
            << Value::temporal(ValueKind::Date, zero) << ValueKind::Date;
        owl::Temporal plusInf;
        plusInf.special = owl::Temporal::Special::PlusInfinity;
        plusInf.original = u"infinity"_s;
        QTest::newRow("date +infinity")
            << Value::temporal(ValueKind::Date, plusInf) << ValueKind::Date;
        QTest::newRow("time micros")
            << Value::time(micros(13, 45, 12, 123456), u"13:45:12.123456"_s) << ValueKind::Time;
        QTest::newRow("timetz +05:30")
            << Value::timeTz(micros(8, 0, 0, 0), 19800, u"08:00:00+05:30"_s) << ValueKind::TimeTz;
        QTest::newRow("timestamp") << Value::timestamp(QDate(2026, 1, 2), micros(3, 4, 5, 678901),
                                                       u"2026-01-02 03:04:05.678901"_s)
                                   << ValueKind::Timestamp;
        QTest::newRow("timestamptz -03:00")
            << Value::timestampTz(QDate(1999, 12, 31), micros(23, 59, 59, 999999), -10800,
                                  u"1999-12-31 23:59:59.999999-03"_s)
            << ValueKind::TimestampTz;
        owl::Temporal minusInf;
        minusInf.special = owl::Temporal::Special::MinusInfinity;
        minusInf.offsetSeconds = 0;
        minusInf.original = u"-infinity"_s;
        QTest::newRow("timestamptz -infinity")
            << Value::temporal(ValueKind::TimestampTz, minusInf) << ValueKind::TimestampTz;

        owl::Interval interval;
        interval.months = 14;
        interval.days = 3;
        interval.micros = micros(4, 5, 6, 789000);
        interval.original = u"1 year 2 mons 3 days 04:05:06.789"_s;
        QTest::newRow("interval") << Value::interval(interval) << ValueKind::Interval;

        QTest::newRow("array empty") << Value::array({}) << ValueKind::Array;
        QTest::newRow("array nested")
            << Value::array({Value::fromInt(1), Value::text(u"x"_s), Value(),
                             Value::array({Value::fromBool(true), Value::fromDouble(2.5)})})
            << ValueKind::Array;

        Value truncatedText = Value::text(u"first 64 KB…"_s);
        truncatedText.setTruncated(10'000'000);
        QTest::newRow("text truncated") << truncatedText << ValueKind::Text;
        Value truncatedBytes = Value::bytes(QByteArray(16, 'x'));
        truncatedBytes.setTruncated(5'000'000);
        QTest::newRow("bytes truncated") << truncatedBytes << ValueKind::Bytes;
    }

    void roundTrip()
    {
        QFETCH(Value, value);
        QFETCH(ValueKind, kind);
        QCOMPARE(value.kind(), kind);
        const std::optional<Value> decoded = cborRoundTrip(value);
        QVERIFY(decoded.has_value());
        const Value back = decoded.value_or(Value());
        QCOMPARE(back.kind(), kind);
        QVERIFY(back == value);
        QCOMPARE(back.isTruncated(), value.isTruncated());
        QCOMPARE(back.fullSize(), value.fullSize());
    }

    void accessors()
    {
        QCOMPARE(Value::fromUInt(18446744073709551615ULL).toUInt(), 18446744073709551615ULL);
        QCOMPARE(Value::decimal(u"0.10"_s).toText(), u"0.10"_s); // trailing zero kept
        QCOMPARE(Value::time(micros(1, 2, 3, 456789), u"01:02:03.456789"_s).toTemporal().timeMicros,
                 micros(1, 2, 3, 456789));
        QCOMPARE(Value::timestampTz(QDate(2026, 1, 1), 0, 3600, u"x"_s).toTemporal().offsetSeconds,
                 std::optional<int>(3600));
        QCOMPARE(Value::date(QDate(2026, 1, 1), u"2026-01-01"_s).toText(), u"2026-01-01"_s);
        QVERIFY(Value::date(QDate(2026, 1, 1)).isTemporal());
        QCOMPARE(Value::array({Value::fromInt(7)}).toArray().value(0).toInt(), 7);

        // Wrong kind -> neutral defaults, never a crash.
        const Value t = Value::text(u"42"_s);
        QCOMPARE(t.toInt(), 0);
        QCOMPARE(t.toBool(), false);
        QVERIFY(t.toBytes().isEmpty());
        QVERIFY(t.toArray().isEmpty());
        QVERIFY(!t.toTemporal().date.isValid());
        QVERIFY(Value().toText().isNull());
        QVERIFY(!Value::fromInt(1).isTruncated());
        QCOMPARE(owl::kindName(ValueKind::TimestampTz), u"timestamptz"_s);
    }

    void equality()
    {
        QVERIFY(Value::fromInt(1) == Value::fromInt(1));
        QVERIFY(!(Value::fromInt(1) == Value::fromUInt(1))); // different kinds
        QVERIFY(!(Value::text(u"1"_s) == Value::decimal(u"1"_s))); // different kinds
        QVERIFY(!(Value::json(u"{}"_s) == Value::text(u"{}"_s)));
        Value truncated = Value::text(u"a"_s);
        truncated.setTruncated(100);
        QVERIFY(!(truncated == Value::text(u"a"_s)));
        QVERIFY(!(Value::date(QDate(2026, 1, 1)) == Value::date(QDate(2026, 1, 2))));
    }

    void malformedValuesAreRejected_data()
    {
        QTest::addColumn<QCborValue>("cbor");
        const auto arr = [](std::initializer_list<QCborValue> items) {
            return QCborValue(QCborArray(items));
        };
        QTest::newRow("not an array") << QCborValue(42);
        QTest::newRow("empty array") << arr({});
        QTest::newRow("kind not integer") << arr({u"6"_s, u"x"_s});
        QTest::newRow("unknown kind") << arr({99, u"x"_s});
        QTest::newRow("negative kind") << arr({-1});
        QTest::newRow("missing payload") << arr({static_cast<int>(ValueKind::Text)});
        QTest::newRow("too many elements") << arr({static_cast<int>(ValueKind::Int), 1, 2, 3});
        QTest::newRow("int as string") << arr({static_cast<int>(ValueKind::Int), u"1"_s});
        QTest::newRow("uint negative") << arr({static_cast<int>(ValueKind::UInt), -1});
        QTest::newRow("uint not a number") << arr({static_cast<int>(ValueKind::UInt), u"abc"_s});
        QTest::newRow("uuid wrong length")
            << arr({static_cast<int>(ValueKind::Uuid), QByteArray(15, 'x')});
        QTest::newRow("bytes as text") << arr({static_cast<int>(ValueKind::Bytes), u"x"_s});
        QTest::newRow("temporal bad special")
            << arr({static_cast<int>(ValueKind::Date), u""_s, QCborValue(nullptr),
                    QCborValue(nullptr), QCborValue(nullptr), 7});
        QTest::newRow("interval missing parts")
            << arr({static_cast<int>(ValueKind::Interval), u"1 day"_s, 0});
        QTest::newRow("array with bad element")
            << arr({static_cast<int>(ValueKind::Array), QCborArray{QCborArray{99}}});
        QTest::newRow("negative fullSize") << arr({static_cast<int>(ValueKind::Text), u"x"_s, -5});
    }

    void malformedValuesAreRejected()
    {
        QFETCH(QCborValue, cbor);
        QVERIFY(!Value::fromCbor(cbor).has_value());
    }

    void columnInfoRoundTrip()
    {
        owl::ColumnInfo c;
        c.name = u"created_at"_s;
        c.kind = ValueKind::TimestampTz;
        c.nativeType = u"timestamp(3) with time zone"_s;
        c.nativeTypeId = 1184;
        c.precision = 3;
        c.nullable = false;
        c.schema = u"app"_s;
        c.table = u"users"_s;
        c.tableColumn = u"created_at"_s;
        c.isPrimaryKey = false;
        QCOMPARE(cborRoundTrip(c), std::optional(c));

        owl::ColumnInfo big;
        big.nativeTypeId = 0xFFFFFFFFu;
        QCOMPARE(cborRoundTrip(big), std::optional(big));

        // Absent keys keep defaults (forward compatible); wrong types are rejected.
        QCborMap partial;
        partial.insert(u"n"_s, u"id"_s);
        const auto decoded = owl::ColumnInfo::fromCbor(partial);
        QVERIFY(decoded);
        const owl::ColumnInfo column = decoded.value_or(owl::ColumnInfo{});
        QCOMPARE(column.name, u"id"_s);
        QCOMPARE(column.kind, ValueKind::Unknown);
        QVERIFY(column.nullable);
        QCborMap wrong;
        wrong.insert(u"pk"_s, u"yes"_s);
        QVERIFY(!owl::ColumnInfo::fromCbor(wrong));
        QCborMap badKind;
        badKind.insert(u"k"_s, 1000);
        QVERIFY(!owl::ColumnInfo::fromCbor(badKind));
    }

    void rowBatchRoundTrip()
    {
        owl::RowBatch batch;
        batch.firstRow = 1000;
        batch.columnCount = 3;
        Value longText = Value::text(u"preview"_s);
        longText.setTruncated(200'000);
        for (int r = 0; r < 4; ++r)
            batch.cells << Value::fromInt(r) << (r == 2 ? Value() : Value::text(u"row %1"_s.arg(r)))
                        << longText;
        QCOMPARE(batch.rowCount(), 4);
        QCOMPARE(batch.at(1, 1).toText(), u"row 1"_s);
        QVERIFY(batch.at(2, 1).isNull());

        const auto decoded = cborRoundTrip(batch);
        QVERIFY(decoded);
        const owl::RowBatch back = decoded.value_or(owl::RowBatch{});
        QVERIFY(back == batch);
        QCOMPARE(back.rowCount(), 4);
        QVERIFY(back.at(3, 2).isTruncated());

        QVERIFY(!owl::RowBatch::fromCbor(QCborArray{0, 2, QCborArray{Value().toCbor()}}));
        QVERIFY(!owl::RowBatch::fromCbor(QCborArray{-1, 1, QCborArray{}}));
        QVERIFY(!owl::RowBatch::fromCbor(QCborArray{0, 1, QCborArray{QCborArray{99}}}));
    }

    void errorsAndMessages()
    {
        owl::DbError e;
        e.category = owl::DbError::Category::Syntax;
        e.sqlState = u"42P01"_s;
        e.nativeCode = 7;
        e.message = u"relation \"users\" does not exist"_s;
        e.detail = u"detail"_s;
        e.hint = u"Did you mean \"user\"?"_s;
        e.position = 15;
        QCOMPARE(cborRoundTrip(e), std::optional(e));
        QCOMPARE(e.toString(), u"Syntax: relation \"users\" does not exist (42P01)"_s);

        QCborMap badCategory;
        badCategory.insert(u"c"_s, 42);
        QVERIFY(!owl::DbError::fromCbor(badCategory));

        owl::ServerMessage m;
        m.severity = owl::ServerMessage::Severity::Warning;
        m.text = u"there is no transaction in progress"_s;
        m.sqlState = u"25P01"_s;
        QCOMPARE(cborRoundTrip(m), std::optional(m));
    }

    void resultType()
    {
        const auto divide = [](int a, int b) -> owl::Result<int> {
            if (b == 0)
                return owl::fail(owl::DbError::Category::Other, u"division by zero"_s);
            return a / b;
        };
        const owl::Result<int> ok = divide(84, 2);
        QVERIFY(ok.has_value());
        QCOMPARE(*ok, 42);
        const owl::Result<int> bad = divide(1, 0);
        QVERIFY(!bad);
        QCOMPARE(bad.error().message, u"division by zero"_s);

        const owl::Result<void> done{};
        QVERIFY(done.has_value());
    }
};

QTEST_GUILESS_MAIN(TestCoreTypes)
#include "tst_core_types.moc"

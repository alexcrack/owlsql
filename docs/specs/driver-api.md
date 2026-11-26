# Spec: Driver API (draft v0.1)

Status: **draft**, accepted decisions 042–053 (see `../decisions.md`).
Scope: everything between the app layer and native client libraries — value model, driver plugin
interfaces, async execution facade, catalog, dialect, errors, logging, reconnect.

---

## 1. Overview

```
 QML ──► App layer (QObject facade, main thread)
           Session ─┬─ ConnectionWorker "meta"      (tree, metadata, grid browse)
                    ├─ ConnectionWorker "tab:<id>"   (one per query tab, lazy)
                    └─ ConnectionWorker "meta:<db>"  (PostgreSQL: one per browsed database)
                         │  each worker = 1 QThread + 1 physical connection + job queue
                         ▼
 OwlCore (no QObject in driver interfaces; blocking, single-threaded per connection)
           IDriver ──► IConnection ──► IResultCursor
                   ──► ICatalog (via IConnection)
                   ──► IDialect (pure, thread-safe, no connection)
                         │
 Driver plugins:  postgres (libpq) · mysql (MariaDB Connector/C) · sqlite (sqlite3)
```

Design rules:

1. **Drivers are blocking.** Driver code is plain, synchronous calls into the native library.
   An `IConnection` is only ever used from the one worker thread that owns it.
2. **Cancellation is the only cross-thread call**, via `ICancelHandle`, which must be thread-safe.
3. **Everything that crosses the core/app boundary is a value type**, copyable and CBOR-encodable,
   so a future OwlSql Gateway can carry it over a WebSocket unchanged (decision 009).
4. **Dialects are pure.** No I/O; safe to call from any thread (the editor uses them for splitting,
   highlighting hints and completion).
5. **No QtSql.**

---

## 2. Value model

```cpp
namespace owl {

enum class ValueKind : quint8 {
    Null, Bool, Int, UInt, Double, Decimal, Text, Bytes,
    Date, Time, TimeTz, Timestamp, TimestampTz, Interval,
    Uuid, Json, Array, Unknown
};

class Value {
public:
    ValueKind kind() const;
    bool isNull() const;
    bool isTruncated() const;        // grid preview cut at limit (see §6.3)
    qint64 fullSize() const;         // byte length on server if known, -1 otherwise

    // typed accessors (assert on wrong kind)
    bool        toBool() const;
    qint64      toInt() const;
    quint64     toUInt() const;      // MySQL BIGINT UNSIGNED
    double      toDouble() const;
    QString     toText() const;      // Text, Json, Decimal (exact textual form), Unknown
    QByteArray  toBytes() const;
    QDate / QTime / QDateTime ...    // + original text kept for tz/precision fidelity
    QList<Value> toArray() const;

    // (displayText moved out of Value: formatting for the grid lives in the app layer — decision 135)
    QCborValue toCbor() const;
    static Value fromCbor(const QCborValue&);
};
```

Rules:
- **Decimals are exact text** (`"12345.678900"`), never `double`.
- Temporal values keep the server's textual form alongside the parsed value
  (precision beyond ms, infinity, BC dates, MySQL zero dates `0000-00-00`).
- `Unknown` = anything without a dedicated kind (PG geometric, ranges, custom enums/domains...):
  stored as server text + native type; editable as text.
- `ColumnInfo` carries the native type, so the UI can choose a cell editor (enum list, JSON editor...).

```cpp
struct ColumnInfo {
    QString   name;
    ValueKind kind;
    QString   nativeType;      // "jsonb", "varchar(255)", "tinyint(1) unsigned"
    quint32   nativeTypeId;    // PG oid / MySQL field type / 0
    int       precision = -1, scale = -1, length = -1;
    bool      nullable = true;
    // origin (when the server tells us) — enables editing query results
    QString   schema, table, tableColumn;
    bool      isPrimaryKey = false;
};

struct RowBatch {
    qsizetype      firstRow;   // absolute index within the result
    QList<Value>   cells;      // row-major, rows * columns
    qsizetype      rowCount;
};
```

---

### 2.1 Implementation and CBOR wire format (M1.3)

Implemented in `src/core/include/owl/core/{value,resultset,dberror,result}.h`.

- `Value` is a `std::variant` with **one alternative per kind** (e.g. `DecimalText` vs `QString`),
  so the variant index identifies the kind; temporal, interval and array payloads sit behind a
  shared immutable pointer. `sizeof(Value)` is **40 bytes** (enforced by a test) — decision 133.
- Temporal kinds store parsed parts *and* the server's original text: `QDate date`,
  `timeMicros` (µs since midnight), optional `offsetSeconds`, `special` (±infinity), `original`.
- **CBOR wire format v1** (decision 134). `ValueKind` and `DbError::Category` numbers are wire
  codes: never renumber, only append.

| Type | Encoding |
|---|---|
| `Value` | array `[kind, payload…, fullSize?]` — trailing `fullSize` only when truncated |
| scalar / text / bytes / uuid | `[k, value]`; uint above INT64_MAX as a decimal string; uuid as 16-byte string |
| temporal kinds | `[k, original, julianDay\|null, timeMicros\|null, offsetSeconds\|null, special]` |
| interval | `[k, original, months, days, micros]` |
| array | `[k, [element…]]` |
| `RowBatch` | `[firstRow, columnCount, [cell…]]` |
| `ColumnInfo`, `DbError`, `ServerMessage` | maps with short keys; absent keys keep defaults (forward compatible), wrong types are rejected |

## 3. Driver plugin interfaces

> **Implemented (M1.4)** in `owl/core/driver.h` and `driverregistry.h`: `IDriver`, `IConnection`,
> `IResultCursor`, `ICancelHandle`, `ConnectionParams`, `Statement`, `ExecOptions`, `ServerInfo`,
> `Capabilities`. `IDriver::dialect()`/`connectionForm()`/`parseUrl()` arrive with the dialect (M1.5)
> and sessions (M1.8); `IConnection::catalog()` with the catalog (M1.7).
> Plugin rules (decision 138): the registry reads `coreApi` from the plugin metadata **before
> loading** and rejects mismatches; plugins link OwlCore statically, so OwlCore must not keep global
> mutable state (each plugin has its own copy).

Packaged as **shared plugins** loaded with `QPluginLoader` (decision 049).
The ABI is internal: plugins are built in the same tree with the same compiler and Qt;
the core refuses plugins whose `coreApi` differs.

```cpp
#define OWL_DRIVER_IID "org.owlsql.Driver/1"

// plugin metadata JSON: { "id": "postgres", "name": "PostgreSQL", "coreApi": 1 }

class IDriver {
public:
    virtual ~IDriver() = default;
    virtual QString id() const = 0;
    virtual QString displayName() const = 0;
    virtual Capabilities capabilities() const = 0;
    virtual ConnectionFormSchema connectionForm() const = 0;   // drives the session dialog
    virtual std::optional<ConnectionParams> parseUrl(QStringView url) const = 0;
    virtual QList<ConnectionParams> importLocalConfig() const = 0;   // .pgpass, .my.cnf
    virtual const IDialect& dialect() const = 0;
    virtual std::unique_ptr<IConnection> createConnection() const = 0;
};
Q_DECLARE_INTERFACE(owl::IDriver, OWL_DRIVER_IID)
```

### 3.1 Connection parameters

```cpp
struct ConnectionParams {
    QString driverId;
    enum class Transport { Tcp, Socket } transport = Transport::Tcp;
    QString host; quint16 port = 0; QString socketPath;
    QString user; QString database;
    SecretRef password;                 // resolved from keychain by app layer just before open
    SslOptions ssl;                     // mode, CA, client cert/key, verify-hostname
    QVariantMap driverOptions;          // per-driver extras declared in connectionForm()
    QString applicationName = "OwlSql";
    std::chrono::seconds connectTimeout{15};
};
```

SSH tunnels are **not** a driver concern: the app layer starts the tunnel (libssh2), then hands the
driver `host=127.0.0.1, port=<local>`.

### 3.2 Connection (blocking, single thread)

```cpp
class IConnection {
public:
    virtual ~IConnection() = default;

    virtual Result<void> open(const ConnectionParams&) = 0;
    virtual void close() = 0;
    virtual bool ping() = 0;                               // cheap liveness check
    virtual ServerInfo serverInfo() const = 0;             // product, version, flavor (MySQL/MariaDB)

    virtual std::shared_ptr<ICancelHandle> cancelHandle() = 0;   // thread-safe object

    virtual Result<std::unique_ptr<IResultCursor>>
        execute(const Statement&, const ExecOptions&) = 0;

    // transaction & session state
    virtual Result<void> setAutocommit(bool) = 0;
    virtual TxState txState() const = 0;                   // Idle, InTransaction, Failed
    virtual Result<void> useDatabase(const QString&) = 0;  // MySQL: USE; PG: Unsupported
    virtual Result<void> setSearchPath(const QStringList&) = 0;

    virtual ICatalog& catalog() = 0;

    // server messages (NOTICE, warnings) produced during the last call
    virtual QList<ServerMessage> takeMessages() = 0;
};

struct Statement {
    QString sql;
    QList<Value> params;          // bound positionally; empty = simple query
    StatementOrigin origin;       // User, Internal (catalog), Browse — for the log
};

struct ExecOptions {
    std::optional<std::chrono::milliseconds> timeout;
    qsizetype fetchChunk = 1000;
    int previewLimitBytes = 64 * 1024;       // truncation for large values (§6.3)
    bool readOnly = false;                    // prod guard: driver sets session/tx read-only
};

class IResultCursor {
public:
    virtual ~IResultCursor() = default;
    virtual QList<ColumnInfo> columns() const = 0;   // empty for non-row statements
    virtual Result<RowBatch> fetch(qsizetype maxRows) = 0;   // empty batch = end
    virtual bool atEnd() const = 0;
    virtual qint64 affectedRows() const = 0;         // -1 if n/a
    virtual std::optional<Value> lastInsertId() const = 0;
    virtual bool nextResult() = 0;                   // MySQL CALL may return several result sets
    virtual void close() = 0;                        // discards the rest (may cancel / drain)
};

class ICancelHandle {                 // the only API callable from other threads
public:
    virtual ~ICancelHandle() = default;
    virtual Result<void> cancel() = 0;
};
```

### 3.3 Errors and messages

```cpp
struct DbError {
    enum class Category { Connection, Auth, Syntax, Constraint, Permission,
                          Timeout, Cancelled, Lost, Unsupported, Other };
    Category category;
    QString  sqlState;      // "42P01", "HY000"
    int      nativeCode = 0;
    QString  message, detail, hint;
    int      position = -1; // char offset in statement → editor squiggle
};

template<class T> using Result = std::expected<T, DbError>;   // C++20 build: tl::expected or own impl

struct ServerMessage { enum Severity { Debug, Info, Notice, Warning } severity; QString text; QString sqlState; };
```

`Category::Lost` is what triggers the reconnect policy (§7).

---

## 4. Catalog

The catalog describes **what** the tree shows; the UI renders it generically.

```cpp
struct ObjectPath { QList<ObjectRef> parts; };    // e.g. [db:app, schema:public, table:users]
struct ObjectRef  { QString kind; QString name; QString oid; };   // oid optional, engine-specific

struct ObjectNode {
    ObjectRef ref;
    QString   displayName;
    QString   icon;               // Tabler icon name
    bool      hasChildren;
    bool      isSystem;           // pg_catalog, information_schema, mysql, sys...
    QVariantMap badges;           // row estimate, size, comment
};

class ICatalog {
public:
    virtual ~ICatalog() = default;
    virtual QList<NodeKindInfo> nodeKinds() const = 0;       // "tables", "matviews", "sequences"...
    virtual Result<QList<ObjectNode>> children(const ObjectPath&) = 0;
    virtual Result<TableDef>   table(const ObjectPath&) = 0;
    virtual Result<ViewDef>    view(const ObjectPath&) = 0;
    virtual Result<RoutineDef> routine(const ObjectPath&) = 0;
    virtual Result<QString>    ddl(const ObjectPath&) = 0;   // "CREATE ..." text for the DDL tab
    virtual Result<QList<ObjectNode>> search(const SearchQuery&) = 0;   // command palette / find
    // ... roles, sequences, types, extensions, triggers, indexes as the editors need them
};
```

`TableDef` (columns, PK, indexes, FKs, checks, uniques, triggers, partitions, comment, engine
options) is the shared model for the table editor, grid editing (PK detection) and DDL generation.

**Caching** (decision 050): the app-layer `CatalogCache` loads nodes lazily on expand and caches
them per session. Invalidation: any DDL executed by OwlSql invalidates affected paths
(dialect's `classify()` reports touched objects where it can; otherwise the parent schema);
F5 refreshes the selected node. No background polling.

---

## 5. Dialect

> **Implemented (M1.5)** in `owl/core/dialect.h` + `sqltokenizer.h`: `IDialect` with `quoteIdentifier`,
> `quoteLiteral`, `split`, `classify`, `placeholders`, `tokenize`; `SqlDialect` implements them from
> lexer options (identifier quotes, dollar quotes, nested comments, placeholder forms) and engines
> subclass it. `split` keeps `;` inside strings, identifiers, comments and compound bodies
> (`CREATE TRIGGER … BEGIN … END`, `BEGIN ATOMIC`). `classify` (decision 142): UPDATE/DELETE without a
> top-level `WHERE` and `TRUNCATE` are `WriteUnbounded`; data-modifying CTEs are `Write`; plain
> `EXPLAIN` is `Read` (`EXPLAIN ANALYZE` takes the inner class); `CALL`/`DO`/`VACUUM` are `Write`.
> Type lists, keywords and SQL generation arrive with their consumers (M1.7, M1.10, M2).

```cpp
class IDialect {                      // pure, thread-safe
public:
    virtual ~IDialect() = default;
    virtual QString quoteIdentifier(QStringView) const = 0;
    virtual QString quoteLiteral(const Value&) const = 0;
    virtual QList<TypeInfo> types(const ServerInfo&) const = 0;   // for editors
    virtual const KeywordSet& keywords() const = 0;
    virtual PlaceholderStyle placeholderStyle() const = 0;       // $1 | ? | ?NNN/:name

    // editor support
    virtual QList<StatementSpan> split(QStringView script) const = 0;
        // handles quotes, comments, PG dollar quotes, MySQL DELIMITER
    virtual StatementClass classify(QStringView stmt) const = 0;
        // Read | Write | WriteUnbounded (UPDATE/DELETE without WHERE) | Ddl | Tx | Other
    virtual QList<Placeholder> placeholders(QStringView stmt) const = 0;

    // SQL generation (ChangeSet → statements; always previewed before running)
    virtual QStringList tableDdl(const TableDef* before, const TableDef& after,
                                 const ServerInfo&) const = 0;      // CREATE or ALTER diff
    virtual QStringList rowChanges(const RowChangeSet&) const = 0;  // INSERT/UPDATE/DELETE by PK
    virtual QString browseQuery(const BrowseRequest&) const = 0;    // SELECT … WHERE … ORDER … LIMIT/OFFSET
};
```

`browseQuery` is used for table browsing (LIMIT/OFFSET pages, WHERE box, column filters, sort).

---

## 6. Async facade (OwlCore)

> **Implemented (M1.4)** in `owl/core/connectionworker.h` / `queryjob.h` — in OwlCore rather than the
> app layer (decision 137): QtCore only, testable without UI, reusable by the Gateway. `Session`
> (roles `meta` / `tab:<id>` / `meta:<db>`) comes with sessions (M1.8). Details as built:
> `columnsReady(columns, resultIndex)`, `resultFinished(resultIndex, affectedRows)`, rows arrive in
> batches of 256 with `firstRow` restarting per result set, `isComplete()` is false when a paused
> job was closed by the next statement, cancelling a *queued* job reports immediately.

### 6.1 Session and workers

`Session` (QObject, main thread) owns the `ConnectionWorker`s for one saved connection:

| Worker role | Opened | Used for |
|---|---|---|
| `meta` | on connect | tree, catalog, DDL tab, grid browsing |
| `meta:<db>` | PostgreSQL only, when another database is expanded | same, per database |
| `tab:<id>` | lazily, on first execution in a query tab | user queries; owns that tab's transaction |

A per-session option **"single connection"** forces every role onto `meta`
for servers with strict connection limits (decision 043).

`ConnectionWorker` = `QThread` + `IConnection` + FIFO job queue. One job runs at a time.

### 6.2 QueryJob

```cpp
class QueryJob : public QObject {
    Q_OBJECT
    QML_UNCREATABLE("")
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(qint64 rowsFetched READ rowsFetched NOTIFY progress)
    Q_PROPERTY(qint64 elapsedMs READ elapsedMs NOTIFY progress)
public:
    enum State { Queued, Running, Paused, Finished, Failed, Cancelled };
    Q_ENUM(State)

    Q_INVOKABLE void fetchMore(qsizetype rows = 1000);   // Paused → Running
    Q_INVOKABLE void fetchAll();
    Q_INVOKABLE void cancel();                            // any state → Cancelled

signals:
    void columnsReady(const QList<owl::ColumnInfo>&);
    void rowsReady(const owl::RowBatch&);
    void message(const owl::ServerMessage&);
    void resultFinished(qint64 affectedRows);              // per result set
    void finished();
    void failed(const owl::DbError&);
    void stateChanged();
    void progress();
};
```

State machine:

```
Queued ─► Running ─► Paused ─(fetchMore)─► Running ─► Finished
              │          │
              │          └──(close/new query in same tab)─► drain/cancel ─► Finished
              ├──► Failed
              └──(cancel)──► Cancelled
```

**Keep stream open** (decision 045): after `fetchChunk` rows the job pauses with the cursor
open; the tab's connection stays busy. Running a new statement in that tab closes the
paused cursor first (drain if cheap, cancel otherwise). Table browsing never pauses — it uses
`browseQuery` pages.

Row batches are delivered with queued signals to a `ResultModel` (`QAbstractTableModel`) that the
QML `TableView` binds to.

### 6.3 Large values

Default preview limit **64 KB** per cell (setting). Drivers truncate client-side while
fetching and set `isTruncated` / `fullSize`. For table browsing the dialect may also truncate
server-side (`left(col, n)`). Opening the cell editor fetches the full value by primary key on
the `meta` worker. Values without a PK origin cannot be re-fetched → the editor shows them read-only.

### 6.4 Placeholders

Before running, the app asks `dialect.placeholders()`. If any, a dialog prompts for values
(remembered per query text hash) and the statement runs with bound `params`
(real prepared/extended-protocol execution, never string interpolation).

---

## 7. Reconnect policy

On `DbError::Category::Lost`:

| Worker state | Behaviour |
|---|---|
| Autocommit, idle, failed job is a **read** | Reconnect silently, restore session state (database, search_path, autocommit), retry once. |
| Failed job is a write | Reconnect, **do not retry**; report the error. |
| Transaction was open | Reconnect, report "Connection lost — transaction rolled back"; tx indicator resets. |

Optional keepalive: `ping()` on idle workers every N minutes (per-session setting, default off).

---

## 8. Logging

Every statement sent by any worker emits a `LogEvent`:

```cpp
struct LogEvent {
    QDateTime at; QUuid sessionId; QString workerRole;
    enum Kind { User, Internal, Browse, Notice, Error } kind;
    QString sql; std::chrono::milliseconds duration; qint64 rows; std::optional<DbError> error;
};
```

The Log panel shows **everything**, filterable by kind (decision 053).
`User` events are also written to persistent History (decision 037).

---

## 9. Safety guards (prod sessions)

Enforced by the app layer using `dialect.classify()` before a job is queued:
- `Write` / `Ddl` → confirmation dialog on prod sessions.
- `WriteUnbounded` → blocked on prod (explicit override in dialog), warning elsewhere.
- Optional per-session **read-only** → `ExecOptions.readOnly` → driver sets
  `default_transaction_read_only` (PG), `SET SESSION TRANSACTION READ ONLY` (MySQL),
  `PRAGMA query_only` (SQLite).

---

## 10. Capabilities

```cpp
enum class Capability {
    Schemas, MultipleDatabases, DatabaseSwitchInSession, TransactionalDdl,
    Returning, MultipleResultSets, Sequences, MaterializedViews, Extensions,
    Roles, RowLevelSecurity, Partitioning, StoredProcedures, Triggers,
    ServerSideCancel, JsonType, ArrayTypes, Enums, Domains, Comments
};
```

The UI hides or disables features from capabilities, never from `driverId` checks.

---

## 11. Engine notes

> **SQLite (M1.5):** `database` = file path, `:memory:` or `file:` URI; `driverOptions` `create`, `readOnly`.
> One statement per `execute()` (decision 143); the first `sqlite3_step` runs in `execute()` so errors
> and affected rows are immediate. Values typed per cell from the storage class (`BOOLEAN`/`JSON`
> declared columns → bool/json). Read-only via `sqlite3_stmt_readonly`; cancel via
> `sqlite3_interrupt`; error positions from `sqlite3_error_offset` (bytes → UTF-16). vcpkg's sqlite3 has
> no `SQLITE_ENABLE_COLUMN_METADATA` option, so column origins are empty (follow-up for editable query
> results, M2). `setAutocommit(false)` → Unsupported until transactions (M2).

| | PostgreSQL | MySQL / MariaDB | SQLite |
|---|---|---|---|
| Client lib | libpq (vcpkg) | MariaDB Connector/C (talks to both) | sqlite3 |
| Min server | 13 | MySQL **5.7**, MariaDB 10.6 | 3.35 (bundled) |
| Streaming | `PQsetChunkedRowsMode` (libpq ≥ 17), fallback single-row mode | `mysql_use_result` (pause = stop reading) | `sqlite3_step` |
| Params | `PQexecParams` (`$1`) | prepared statements (`?`) | `sqlite3_bind_*` (`?`, `:name`) |
| Cancel | `PQcancelConn` / `PQcancel` | `KILL QUERY <id>` via short-lived side connection | `sqlite3_interrupt` |
| Switch DB | new connection (`meta:<db>`) | `USE` | `ATTACH` |
| Multi results | n/a (statements split client-side) | yes (`CALL`) | n/a |
| Read-only | `default_transaction_read_only` | `SET SESSION TRANSACTION READ ONLY` | `PRAGMA query_only` / `SQLITE_OPEN_READONLY` |
| Notes | DDL is transactional | 5.7 lacks CTEs, window functions, CHECK; catalog queries need version branches | Single writer; ALTER TABLE limited → dialect uses table-rebuild recipe |

---

## 12. Testing

> **Conformance suite implemented (M1.5):** `tests/conformance/` — `owl_conformance` library + one small
> executable per driver supplying SQL fixtures. Loads the driver **as a plugin through DriverRegistry**.
> Checks: server info, scalar types (incl. empty text vs NULL, bytes, 2^62), bound parameters and
> count mismatch, syntax error category + position, constraint violation, affected rows, transaction
> state, large-value truncation, read-only guard, empty statement, streaming 10 000 rows with pause /
> fetchAll through the worker, cancelling a runaway query. ctest label `conformance`.

- **Unit (no server):** Value/CBOR round-trips, dialect split/classify/quote/placeholders, DDL diff
  generation (golden files per engine), browse query generation.
- **Integration (Docker on CI):** PostgreSQL 13 + latest, MySQL 5.7 + 8.x, MariaDB 10.6 + latest,
  SQLite. A shared conformance suite every driver must pass (types round-trip, streaming + pause,
  cancel, error mapping, catalog of a fixture schema).

---

## 13. Resolved / open questions

- ✅ `Result<T>` = `tl::expected` (vcpkg) while on C++20; switch to `std::expected` with C++23 (decision 078).
- ✅ Value types get `toCbor/fromCbor` + round-trip tests from M1; no transport until the Gateway (decision 079).
- ✅ `LISTEN/NOTIFY` is post-1.0 entirely — not part of the v1 API (decision 080).
- ⏳ Exact `ConnectionFormSchema` format (fields, types, groups, validation) — to be specified with the
  session manager UI.

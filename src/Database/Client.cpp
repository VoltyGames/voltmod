#include <VoltMod/Core/Files/Paths.hpp>
#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Time/Scheduler.hpp>
#include <VoltMod/Database/Client.hpp>
#include <filesystem>
#include <optional>

namespace VoltMod
{

/** Jobs beyond this are refused while the database stalls. */
static constexpr size_t MaxQueuedJobs = 4096;
/** After a failed connect, jobs fail at once for this long. */
static constexpr auto ReconnectBackoff = std::chrono::seconds(5);

using PostgresSslMode = sqlpp::postgresql::connection_config::sslmode_t;

static std::optional<PostgresSslMode> ParseSslMode(const std::string& mode)
{
    if (mode == "disable")
    {
        return PostgresSslMode::disable;
    }
    if (mode == "allow")
    {
        return PostgresSslMode::allow;
    }
    if (mode == "prefer")
    {
        return PostgresSslMode::prefer;
    }
    if (mode == "require")
    {
        return PostgresSslMode::require;
    }
    if (mode == "verify-ca")
    {
        return PostgresSslMode::verify_ca;
    }
    if (mode == "verify-full")
    {
        return PostgresSslMode::verify_full;
    }
    return std::nullopt;
}

/** MariaDB takes a bool, not a mode: "require" and stricter mean the connection must use TLS. */
static bool WantsTls(const std::string& sslMode)
{
    const PostgresSslMode mode = ParseSslMode(sslMode).value_or(PostgresSslMode::prefer);
    return mode == PostgresSslMode::require || mode == PostgresSslMode::verify_ca ||
           mode == PostgresSslMode::verify_full;
}

static sqlpp::postgresql::connection_config PostgresSettings(const DatabaseConfig& config)
{
    sqlpp::postgresql::connection_config settings;
    settings.host = config.host;
    settings.port = config.port > 0 ? static_cast<uint32_t>(config.port) : 5432u;
    settings.dbname = config.database;
    settings.user = config.username;
    settings.password = config.password;
    settings.connect_timeout = static_cast<uint32_t>(config.connectTimeoutSec);
    settings.sslmode = ParseSslMode(config.sslMode).value_or(PostgresSslMode::prefer);
    return settings;
}

static sqlpp::mysql::connection_config MariaDbSettings(const DatabaseConfig& config)
{
    sqlpp::mysql::connection_config settings;
    settings.host = config.host;
    settings.port = config.port > 0 ? static_cast<unsigned int>(config.port) : 3306u;
    settings.database = config.database;
    settings.user = config.username;
    settings.password = config.password;
    settings.connect_timeout_seconds = static_cast<unsigned int>(config.connectTimeoutSec);
    settings.charset = "utf8mb4";
    settings.ssl = WantsTls(config.sslMode);
    // Affected rows must mean matched rows, which update-then-insert reads to decide on the insert.
    settings.client_flag |= CLIENT_FOUND_ROWS;
    return settings;
}

static sqlpp::sqlite3::connection_config SqliteSettings(const DatabaseConfig& config)
{
    sqlpp::sqlite3::connection_config settings;
    settings.flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    if (config.path == ":memory:")
    {
        settings.path_to_database = config.path;
        return settings;
    }

    // Relative paths must resolve against the game dir, not the server process cwd.
    const std::filesystem::path file = ResolvePath(config.path);
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    settings.path_to_database = file.string();
    return settings;
}

static bool ConfigIsValid(Driver driver, const DatabaseConfig& config)
{
    if (driver == Driver::Sqlite && config.path.empty())
    {
        Log::Error("Database driver 'sqlite' needs a 'path' (a file, or \":memory:\").");
        return false;
    }
    if (driver == Driver::Postgres && !ParseSslMode(config.sslMode))
    {
        Log::Error("Unknown sslMode '{}'; expected disable, allow, prefer, require, verify-ca or verify-full.",
                   config.sslMode);
        return false;
    }
    return true;
}

Database::~Database()
{
    Disconnect();
}

bool Database::Connect(const DatabaseConfig& config)
{
    auto driver = ParseDriver(config.driver);
    if (!driver)
    {
        Log::Error("Unknown database driver '{}'; expected postgres, mariadb or sqlite.", config.driver);
        return false;
    }
    if (!ConfigIsValid(*driver, config))
    {
        return false;
    }

    {
        std::lock_guard lock(_queueMutex);
        if (_worker.joinable())
        {
            return true;
        }
        _config = config;
        _driver = *driver;
        _accepting = true;
        _stopping = false;
    }

    _worker = std::thread([this] { WorkerMain(); });

    // Typed, not raw: a raw SELECT leaves an unread result set on MariaDB.
    auto ping = Run("db_ping", [](auto& conn) {
        for ([[maybe_unused]] const auto& row : conn(sqlpp::select(sqlpp::value(1).as(sqlpp::alias::a))))
        {}
    });
    if (!ping)
    {
        Disconnect();
        return false;
    }

    _onFrame = _scheduler.EveryFrame([this] { DispatchCompletions(); });
    return true;
}

void Database::Disconnect(std::chrono::milliseconds stopDeadline)
{
    {
        std::lock_guard lock(_queueMutex);
        if (!_worker.joinable() && !_accepting)
        {
            return;
        }
        _accepting = false;
        _stopping = true;
        _stopDeadline = std::chrono::steady_clock::now() + stopDeadline;
    }
    _queueCv.notify_all();

    if (_worker.joinable())
    {
        _worker.join();
    }

    _onFrame.Reset();

    // Dropped unrun: the state they touch is unloading.
    {
        std::lock_guard lock(_completionMutex);
        _completions.clear();
        _hasCompletions.store(false, std::memory_order_relaxed);
    }
}

void Database::DispatchCompletions()
{
    if (!_hasCompletions.load(std::memory_order_acquire))
    {
        return;
    }

    std::vector<std::move_only_function<void()>> ready;
    {
        std::lock_guard lock(_completionMutex);
        ready.swap(_completions);
        _hasCompletions.store(false, std::memory_order_relaxed);
    }
    for (auto& completion : ready)
    {
        completion();
    }
}

void Database::PushCompletion(std::move_only_function<void()> completion)
{
    std::lock_guard lock(_completionMutex);
    _completions.push_back(std::move(completion));
    _hasCompletions.store(true, std::memory_order_release);
}

void Database::Enqueue(Job job)
{
    bool running = false;
    bool full = false;
    {
        std::lock_guard lock(_queueMutex);
        running = _accepting;
        full = _queue.size() >= MaxQueuedJobs;
        if (running && !full)
        {
            _queue.push_back(std::move(job));
        }
    }

    if (running && !full)
    {
        _queueCv.notify_all();
        return;
    }

    const std::string_view reason = running ? "the job queue is full" : "database not running";
    Log::Warn("db: '{}' failed - {}.", job.Name, reason);

    // OnFail delivers on a later frame, so the caller is never re-entered here.
    job.OnFail(Error::NotReady(std::string(reason)));
}

void Database::WorkerMain()
{
    while (std::optional<Job> job = TakeJob())
    {
        RunJob(*job);
    }

    DropConnection();
}

std::optional<Database::Job> Database::TakeJob()
{
    std::unique_lock lock(_queueMutex);
    _queueCv.wait(lock, [&] { return !_queue.empty() || _stopping; });

    if (_queue.empty() && _stopping)
    {
        return std::nullopt;
    }

    // A dead database must not hang the unload.
    if (_stopping && std::chrono::steady_clock::now() >= _stopDeadline)
    {
        for (auto& dropped : _queue)
        {
            Log::Warn("db: dropping queued '{}' - shutdown stop deadline reached.", dropped.Name);
            dropped.OnFail(Error::NotReady("shutdown"));
        }
        _queue.clear();
        return std::nullopt;
    }

    Job job = std::move(_queue.front());
    _queue.pop_front();
    return job;
}

void Database::RunJob(Job& job)
{
    if (!EnsureOpen())
    {
        Log::Error("db: '{}' failed - no database connection.", job.Name);
        job.OnFail(Error::NotReady("no database connection"));
        return;
    }

    try
    {
        job.Run(_connection);
    }
    catch (const std::exception& e)
    {
        Log::Error("db: {} failed: {}", job.Name, e.what());
        DropConnection();  // its state is unknown
        job.OnFail(Error::Failed(e.what()));
    }
}

bool Database::EnsureOpen()
{
    const bool open = std::visit(
        [](const auto& conn) {
            if constexpr (std::same_as<std::remove_cvref_t<decltype(conn)>, std::monostate>)
            {
                return false;
            }
            else
            {
                return conn.is_connected();
            }
        },
        _connection);
    if (open)
    {
        return true;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now < _retryAt)
    {
        return false;
    }

    try
    {
        switch (_driver)
        {
        case Driver::Postgres:
            _connection.emplace<PostgresConnection>(PostgresSettings(_config));
            break;
        case Driver::MariaDb:
            sqlpp::mysql::global_library_init();
            _connection.emplace<MariaDbConnection>(MariaDbSettings(_config));
            break;
        case Driver::Sqlite:
        {
            auto& conn = _connection.emplace<SqliteConnection>(SqliteSettings(_config));
            if (_config.path != ":memory:")
            {
                conn("PRAGMA journal_mode=WAL");
            }
            conn("PRAGMA busy_timeout=" + std::to_string(_config.connectTimeoutSec * 1000));
            break;
        }
        }
        _connected.store(true, std::memory_order_relaxed);
        return true;
    }
    catch (const std::exception&)
    {
        // The exception text can echo the connection string, password included.
        Log::Error("Database connection failed - check host/port/credentials.");
    }
    DropConnection();
    _retryAt = now + ReconnectBackoff;
    return false;
}

void Database::DropConnection()
{
    _connection.emplace<std::monostate>();
    _connected.store(false, std::memory_order_relaxed);
}

}  // namespace VoltMod

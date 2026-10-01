#include "Database/OpenConnection.hpp"

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Database/Client.hpp>

namespace VoltMod
{

/** Jobs beyond this are refused while the database stalls. */
static constexpr size_t MaxQueuedJobs = 4096;
/** After a failed connect, jobs fail at once for this long. */
static constexpr auto ReconnectBackoff = std::chrono::seconds(5);

Database::~Database()
{
    Disconnect();
}

bool Database::Connect(const DatabaseConfig& config)
{
    const Result<Driver> driver = CheckConfig(config);
    if (!driver)
    {
        Log::Error("{}", driver.error().Detail);
        return false;
    }

    {
        std::lock_guard lock(_queueMutex);
        if (_state != State::Stopped)
        {
            return true;
        }
        _config = config;
        _driver = *driver;
        _state = State::Running;
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
        if (_state != State::Running)
        {
            return;
        }
        _state = State::Stopping;
        _stopDeadline = std::chrono::steady_clock::now() + stopDeadline;
    }
    _queueCv.notify_all();
    _worker.join();

    {
        std::lock_guard lock(_queueMutex);
        _state = State::Stopped;
    }
    _onFrame.Reset();
    // Dropped unrun: the state they touch is unloading.
    _completions.Clear();
}

void Database::Enqueue(Job job)
{
    std::string_view refused;
    {
        std::lock_guard lock(_queueMutex);
        if (_state != State::Running)
        {
            refused = "database not running";
        }
        else if (_queue.size() >= MaxQueuedJobs)
        {
            refused = "the job queue is full";
        }
        else
        {
            _queue.push_back(std::move(job));
        }
    }

    if (refused.empty())
    {
        _queueCv.notify_one();
        return;
    }
    Log::Warn("db: '{}' failed - {}.", job.Name, refused);
    // Fail delivers on a later frame, so the caller is never re-entered here.
    job.Fail(Error::NotReady(std::string(refused)));
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
    _queueCv.wait(lock, [this] { return !_queue.empty() || _state == State::Stopping; });

    // A dead database must not hang the unload.
    const bool pastDeadline = _state == State::Stopping && std::chrono::steady_clock::now() >= _stopDeadline;
    if (pastDeadline)
    {
        for (Job& dropped : _queue)
        {
            Log::Warn("db: dropping queued '{}' - shutdown stop deadline reached.", dropped.Name);
            dropped.Fail(Error::NotReady("shutdown"));
        }
        _queue.clear();
    }
    if (_queue.empty())
    {
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
        job.Fail(Error::NotReady("no database connection"));
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
        job.Fail(Error::Failed(e.what()));
    }
}

bool Database::EnsureOpen()
{
    if (IsOpen(_connection))
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
        OpenConnection(_connection, _driver, _config);
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

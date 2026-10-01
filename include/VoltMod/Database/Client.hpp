#pragma once

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Time/Scheduler.hpp>
#include <VoltMod/Database/Connection.hpp>
#include <VoltMod/Database/DatabaseConfig.hpp>
#include <VoltMod/Database/Driver.hpp>
#include <VoltMod/Database/Migrator.hpp>
#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace VoltMod
{

/**
 * @brief Async database access over Postgres, MariaDB and SQLite.
 *
 * One worker thread owns the connection; jobs run in order and their completions run on the game
 * thread. A job is a callable over `auto& conn` that returns the same type for every driver.
 * @ref Run blocks, so it is for load time; gameplay uses @ref RunAsync.
 */
class Database
{
public:
    /** The worker's connection; monostate while closed. */
    using AnyConnection = std::variant<std::monostate, PostgresConnection, MariaDbConnection, SqliteConnection>;

    /** What a job returns; @ref CheckJob makes every driver agree. */
    template <class Fn>
    using ResultOf = std::invoke_result_t<Fn&, SqliteConnection&>;

    /** @p scheduler drives per-frame completion delivery and must outlive this object. */
    explicit Database(Scheduler& scheduler) : _scheduler(scheduler) {}
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /** Start the worker and ping; false on a bad config or an unreachable database. */
    bool Connect(const DatabaseConfig& config);

    /** Let queued jobs finish within @p stopDeadline, then join the worker; later jobs fail and
     *  undelivered completions are dropped. The destructor calls it. */
    void Disconnect(std::chrono::milliseconds stopDeadline = std::chrono::seconds(5));

    /** Run @p fn on the worker; @p onDone runs on the game thread later. @p name is a log label. */
    template <class Fn>
    void RunAsync(std::string name, Fn fn, std::move_only_function<void(Result<ResultOf<Fn>>)> onDone = {})
    {
        using Value = ResultOf<Fn>;
        CheckJob<Fn>();

        Job job;
        job.Name = std::move(name);
        if (!onDone)
        {
            job.Run = [fn = std::move(fn)](AnyConnection& conn) mutable { Invoke(conn, fn); };
            job.OnFail = [](Error) {};
            Enqueue(std::move(job));
            return;
        }

        // Run and OnFail both hold it; only one of them fires.
        auto callback = std::make_shared<std::move_only_function<void(Result<Value>)>>(std::move(onDone));
        job.Run = [this, fn = std::move(fn), callback](AnyConnection& conn) mutable {
            if constexpr (std::is_void_v<Value>)
            {
                Invoke(conn, fn);
                PushCompletion([callback] { (*callback)(Result<void>{}); });
            }
            else
            {
                PushCompletion(
                    [callback, value = Invoke(conn, fn)]() mutable { (*callback)(Result<Value>{std::move(value)}); });
            }
        };
        job.OnFail = [this, callback](Error error) {
            PushCompletion(
                [callback, error = std::move(error)]() mutable { (*callback)(std::unexpected(std::move(error))); });
        };
        Enqueue(std::move(job));
    }

    /** @ref RunAsync for a callback taking the value alone; a failure is only logged. */
    template <class Fn, class OnValue>
        requires(!std::is_void_v<ResultOf<Fn>> && std::invocable<OnValue&, ResultOf<Fn>> &&
                 !std::invocable<OnValue&, Result<ResultOf<Fn>>>)
    void RunAsync(std::string name, Fn fn, OnValue onValue)
    {
        RunAsync(std::move(name), std::move(fn),
                 std::move_only_function<void(Result<ResultOf<Fn>>)>(
                     [onValue = std::move(onValue)](Result<ResultOf<Fn>> result) mutable {
                         // An empty std::function means no callback.
                         if constexpr (requires { static_cast<bool>(onValue); })
                         {
                             if (!onValue)
                             {
                                 return;
                             }
                         }
                         if (result)
                         {
                             onValue(std::move(*result));
                         }
                     }));
    }

    /** @ref RunAsync that blocks; load time only. */
    template <class Fn>
    Result<ResultOf<Fn>> Run(std::string name, Fn fn)
    {
        using Value = ResultOf<Fn>;
        CheckJob<Fn>();

        auto promise = std::make_shared<std::promise<Result<Value>>>();
        std::future<Result<Value>> answer = promise->get_future();

        Job job;
        job.Name = std::move(name);
        job.Run = [fn = std::move(fn), promise](AnyConnection& conn) mutable {
            if constexpr (std::is_void_v<Value>)
            {
                Invoke(conn, fn);
                promise->set_value(Result<void>{});
            }
            else
            {
                promise->set_value(Result<Value>{Invoke(conn, fn)});
            }
        };
        job.OnFail = [promise](Error error) { promise->set_value(std::unexpected(std::move(error))); };
        Enqueue(std::move(job));

        return answer.get();
    }

    /** @ref Run, falling back to @p fallback when the job fails. */
    template <class Fn>
    ResultOf<Fn> RunOr(std::string name, Fn fn, ResultOf<Fn> fallback = {})
    {
        auto result = Run(std::move(name), std::move(fn));
        return result ? std::move(*result) : std::move(fallback);
    }

    /** Run the finished jobs' completions; @ref Connect schedules this every frame. */
    void DispatchCompletions();

    /** Whether the worker's last job had a live connection; a diagnostic, not a guarantee. */
    bool IsConnected() const { return _connected.load(std::memory_order_relaxed); }

    /** The configured driver, set by @ref Connect. */
    Driver GetDriver() const { return _driver; }

private:
    struct Job
    {
        std::string Name;  ///< log label only
        std::move_only_function<void(AnyConnection&)> Run;
        std::move_only_function<void(Error)> OnFail;
    };

    /** A job must compile and return the same type on every driver. */
    template <class Fn>
    static void CheckJob()
    {
        static_assert(std::same_as<std::invoke_result_t<Fn&, PostgresConnection&>, ResultOf<Fn>> &&
                          std::same_as<std::invoke_result_t<Fn&, MariaDbConnection&>, ResultOf<Fn>>,
                      "a database job must return the same type for every driver");
    }

    template <class Fn>
    static ResultOf<Fn> Invoke(AnyConnection& conn, Fn& fn)
    {
        return std::visit(
            [&fn](auto& open) -> ResultOf<Fn> {
                if constexpr (std::same_as<std::remove_cvref_t<decltype(open)>, std::monostate>)
                {
                    throw std::logic_error("no database connection");
                }
                else
                {
                    return fn(open);
                }
            },
            conn);
    }

    void Enqueue(Job job);
    void PushCompletion(std::move_only_function<void()> completion);
    void WorkerMain();
    /** Waits for the next job; nothing once stopping. */
    std::optional<Job> TakeJob();
    void RunJob(Job& job);
    /** Open the connection if needed; its failure log carries no secrets. */
    bool EnsureOpen();
    /** The next job reopens it. */
    void DropConnection();

    Scheduler& _scheduler;
    DatabaseConfig _config;
    Driver _driver = Driver::Postgres;

    std::mutex _queueMutex;
    std::condition_variable _queueCv;
    std::deque<Job> _queue;
    bool _accepting = false;
    bool _stopping = false;
    std::chrono::steady_clock::time_point _stopDeadline{};

    std::mutex _completionMutex;
    std::vector<std::move_only_function<void()>> _completions;
    /** Read every frame without the lock. */
    std::atomic<bool> _hasCompletions{false};

    std::thread _worker;
    Subscription _onFrame;

    /** Written by the worker, read on the game thread. */
    std::atomic<bool> _connected{false};

    /** Worker thread only. */
    AnyConnection _connection;
    std::chrono::steady_clock::time_point _retryAt{};
};

/**
 * Apply the `NNNN_*.sql` files in `dir` newer than the database's version, in order, each in its own
 * transaction under a lock. @ref ResolveDialect fills their placeholders for the live driver. A
 * missing directory is a no-op; on failure the database stays at the last applied version.
 */
MigrationResult RunMigrations(Database& db, std::string_view dir, const MigrationOptions& options = {});

}  // namespace VoltMod

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Http/HttpClient.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cpr/cpr.h>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace VoltMod
{

static constexpr size_t MaxWorkers = 4;
/** Requests beyond this many waiting fail at once. */
static constexpr size_t MaxWaiting = 64;

static HttpResult ToResult(cpr::Response&& response)
{
    if (response.error)
    {
        return {.Error = std::move(response.error.message)};
    }
    return {.Ok = true, .StatusCode = static_cast<long>(response.status_code), .Body = std::move(response.text)};
}

/** Worker thread: touches nothing but the request. */
static HttpResult Perform(const HttpRequest& request, const std::atomic_bool& stopped)
{
    const cpr::Url url{request.Url};
    const cpr::Header headers(request.Headers.begin(), request.Headers.end());
    const cpr::Timeout timeout{std::chrono::milliseconds{request.TimeoutMs}};
    // False aborts the transfer, so Stop does not wait out a stalled endpoint.
    const cpr::ProgressCallback progress{[&stopped](auto&&...) { return !stopped; }};

    switch (request.Method)
    {
    case HttpMethod::Get:
        return ToResult(cpr::Get(url, headers, timeout, progress));
    case HttpMethod::Post:
        return ToResult(cpr::Post(url, cpr::Body{request.Body}, headers, timeout, progress));
    case HttpMethod::Put:
        return ToResult(cpr::Put(url, cpr::Body{request.Body}, headers, timeout, progress));
    case HttpMethod::Patch:
        return ToResult(cpr::Patch(url, cpr::Body{request.Body}, headers, timeout, progress));
    case HttpMethod::Delete:
        return ToResult(cpr::Delete(url, cpr::Body{request.Body}, headers, timeout, progress));
    }
    return {.Error = "unsupported HTTP method"};
}

struct WaitingRequest
{
    HttpRequest Request;
    HttpCompletion OnComplete;
};

struct FinishedRequest
{
    HttpResult Result;
    HttpCompletion OnComplete;
};

/** Shared with the workers; Mutex guards all but the atomics. */
struct HttpClient::Requests
{
    std::mutex Mutex;
    std::condition_variable Wake;
    std::deque<WaitingRequest> Waiting;
    std::vector<FinishedRequest> Finished;
    std::vector<std::thread> Workers;
    size_t IdleWorkers = 0;
    std::atomic_bool HasFinished = false;
    std::atomic_bool Stopped = false;  // never cleared: its plugin is unloading

    void Work()
    {
        while (std::optional<WaitingRequest> next = Take())
        {
            HttpResult result = Perform(next->Request, Stopped);
            Finish(std::move(result), std::move(next->OnComplete));
        }
    }

    /** Waits for the next request; nothing once stopped. */
    std::optional<WaitingRequest> Take()
    {
        std::unique_lock lock(Mutex);
        ++IdleWorkers;
        Wake.wait(lock, [this] { return Stopped || !Waiting.empty(); });
        --IdleWorkers;
        if (Stopped)
        {
            return std::nullopt;
        }
        WaitingRequest next = std::move(Waiting.front());
        Waiting.pop_front();
        return next;
    }

    void Finish(HttpResult result, HttpCompletion onComplete)
    {
        std::lock_guard lock(Mutex);
        Finished.push_back({std::move(result), std::move(onComplete)});
        HasFinished.store(true, std::memory_order_release);
    }
};

HttpClient::HttpClient(Scheduler& scheduler)
    : _requests(std::make_unique<Requests>()), _onFrame(scheduler.EveryFrame([this] { RunCompletions(); }))
{}

HttpClient::~HttpClient()
{
    Stop();
}

void HttpClient::Send(HttpRequest request, HttpCompletion onComplete)
{
    Requests& requests = *_requests;
    if (requests.Stopped)
    {
        Log::Warn("http: dropped a request to '{}' because the client is stopped.", request.Url);
        return;
    }

    {
        std::lock_guard lock(requests.Mutex);
        if (requests.Waiting.size() >= MaxWaiting)
        {
            Log::Warn("http: refused a request to '{}': {} requests are already waiting.", request.Url, MaxWaiting);
            requests.Finished.push_back({{.Error = "too many requests waiting"}, std::move(onComplete)});
            requests.HasFinished.store(true, std::memory_order_release);
            return;
        }

        requests.Waiting.push_back({std::move(request), std::move(onComplete)});
        // Stop joins every worker before Requests goes away.
        if (requests.IdleWorkers == 0 && requests.Workers.size() < MaxWorkers)
        {
            requests.Workers.emplace_back([&requests] { requests.Work(); });
        }
    }
    requests.Wake.notify_one();
}

void HttpClient::Stop()
{
    Requests& requests = *_requests;
    {
        std::lock_guard lock(requests.Mutex);
        requests.Stopped = true;
    }
    requests.Wake.notify_all();
    for (std::thread& worker : requests.Workers)
    {
        worker.join();
    }

    requests.Workers.clear();
    requests.Waiting.clear();
    requests.Finished.clear();
    requests.HasFinished = false;
}

void HttpClient::RunCompletions()
{
    Requests& requests = *_requests;
    if (!requests.HasFinished.load(std::memory_order_acquire))
    {
        return;
    }

    // Take them out first: a completion may Send.
    std::vector<FinishedRequest> done;
    {
        std::lock_guard lock(requests.Mutex);
        done.swap(requests.Finished);
        requests.HasFinished.store(false, std::memory_order_relaxed);
    }

    for (FinishedRequest& request : done)
    {
        if (request.OnComplete)
        {
            request.OnComplete(request.Result);
        }
    }
}

}  // namespace VoltMod

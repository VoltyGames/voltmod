#pragma once

#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Time/Scheduler.hpp>
#include <VoltMod/Http/HttpResult.hpp>
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace VoltMod
{

enum class HttpMethod
{
    Get,
    Post,
    Put,
    Patch,
    Delete,
};

/** Orders header names ignoring ASCII case, as HTTP compares them. */
struct HeaderNameLess
{
    using is_transparent = void;

    bool operator()(std::string_view left, std::string_view right) const
    {
        return std::ranges::lexicographical_compare(left, right, {}, ToLower, ToLower);
    }

private:
    static char ToLower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }
};

/** Header name -> value; `Content-Type` and `content-type` are one entry. */
using HttpHeaders = std::map<std::string, std::string, HeaderNameLess>;

struct HttpRequest
{
    HttpMethod Method = HttpMethod::Get;
    std::string Url;
    std::string Body;
    HttpHeaders Headers;
    long TimeoutMs = 8000;

    /** Add a credential header, e.g. `AddAuth("Authorization", "Bearer", key)`; an empty @p scheme
     *  sends @p key alone, and an empty @p key adds nothing. */
    void AddAuth(std::string_view header, std::string_view scheme, std::string_view key)
    {
        if (!key.empty())
        {
            Headers[std::string(header)] =
                scheme.empty() ? std::string(key) : std::string(scheme) + " " + std::string(key);
        }
    }
};

/**
 * @brief Runs HTTP requests off the game thread and their completions on it.
 *
 * Up to four run at once on kept worker threads and 64 more may wait; later ones fail.
 */
class HttpClient
{
public:
    /** @p scheduler must outlive the client. */
    explicit HttpClient(Scheduler& scheduler);
    ~HttpClient();
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    /** Run @p request; @p onComplete runs on the game thread on a later frame. Dropped after @ref Stop. */
    void Send(HttpRequest request, HttpCompletion onComplete);

    /** Abort the running requests, drop the waiting ones and join the workers; no completion runs
     *  after this, and later sends are dropped. */
    void Stop();

private:
    /** Every frame. */
    void RunCompletions();

    struct Requests;
    std::unique_ptr<Requests> _requests;
    Subscription _onFrame;  // after _requests, so frames stop before the requests go away
};

}  // namespace VoltMod

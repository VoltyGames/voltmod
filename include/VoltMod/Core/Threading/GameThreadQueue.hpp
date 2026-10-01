#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace VoltMod
{

/** @brief Callbacks worker threads hand to the game thread: @ref Push from any thread, @ref RunAll on
 *  the game thread. */
class GameThreadQueue
{
public:
    void Push(std::move_only_function<void()> completion)
    {
        std::lock_guard lock(_mutex);
        _waiting.push_back(std::move(completion));
        _any.store(true, std::memory_order_release);
    }

    /** Runs what was pushed so far; anything pushed meanwhile waits for the next call. */
    void RunAll()
    {
        // Called every frame, so an empty queue takes no lock.
        if (!_any.load(std::memory_order_acquire))
        {
            return;
        }
        std::vector<std::move_only_function<void()>> ready;
        {
            std::lock_guard lock(_mutex);
            ready.swap(_waiting);
            _any.store(false, std::memory_order_relaxed);
        }
        for (auto& completion : ready)
        {
            completion();
        }
    }

    /** Drops what is waiting, unrun. */
    void Clear()
    {
        std::lock_guard lock(_mutex);
        _waiting.clear();
        _any.store(false, std::memory_order_relaxed);
    }

private:
    std::mutex _mutex;
    std::vector<std::move_only_function<void()>> _waiting;
    std::atomic<bool> _any{false};
};

}  // namespace VoltMod

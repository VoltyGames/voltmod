#pragma once

#include <VoltMod/Core/Result.hpp>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace VoltMod
{

/** A check that failed while a plugin loaded. */
struct FailedCheck
{
    std::string Name;
    std::string Reason;
    bool Required = false;  ///< A required failure refuses the plugin.
};

/**
 * What went wrong while one plugin loaded. The runtime checks its own services here, and plugin
 * members and `Plugin::Load` add theirs. @ref Plugin logs @ref Summary and refuses the load with
 * @ref AbortReason.
 */
class LoadReport
{
public:
    /** Record @p result; a failure disables that feature. Returns whether it succeeded. */
    bool Optional(std::string_view name, Status result);

    /** Record @p result; a failure refuses the plugin. Returns whether it succeeded. */
    bool Required(std::string_view name, Status result);

    /** `Load took N ms`, then one line per failure. */
    std::string Summary() const;

    /** `<name>: <reason>` for the first required failure; empty when none failed. */
    std::string AbortReason() const;

    const std::vector<FailedCheck>& Failures() const { return _failures; }

private:
    bool Record(std::string_view name, Status result, bool required);

    std::vector<FailedCheck> _failures;
    std::chrono::steady_clock::time_point _started = std::chrono::steady_clock::now();
};

}  // namespace VoltMod

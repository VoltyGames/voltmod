#include <VoltMod/Core/LoadReport.hpp>
#include <algorithm>
#include <format>
#include <utility>

namespace VoltMod
{

bool LoadReport::Optional(std::string_view name, Status result)
{
    return Record(name, std::move(result), false);
}

bool LoadReport::Required(std::string_view name, Status result)
{
    return Record(name, std::move(result), true);
}

bool LoadReport::Record(std::string_view name, Status result, bool required)
{
    if (result)
    {
        return true;
    }
    _failures.push_back({.Name = std::string(name), .Reason = std::move(result.error().Detail), .Required = required});
    return false;
}

std::string LoadReport::Summary() const
{
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _started);
    std::string out = std::format("Load took {} ms", elapsed.count());
    if (_failures.empty())
    {
        return out;
    }

    out += std::format("; {} failed:", _failures.size());
    for (const FailedCheck& failed : _failures)
    {
        out += std::format("\n  {} {}: {}", failed.Required ? "required" : "optional", failed.Name, failed.Reason);
    }
    return out;
}

std::string LoadReport::AbortReason() const
{
    const auto it = std::ranges::find(_failures, true, &FailedCheck::Required);
    if (it == _failures.end())
    {
        return {};
    }
    return it->Reason.empty() ? it->Name : std::format("{}: {}", it->Name, it->Reason);
}

}  // namespace VoltMod

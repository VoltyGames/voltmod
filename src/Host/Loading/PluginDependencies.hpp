#pragma once

#include "Host/Loading/InstalledPlugins.hpp"

#include <VoltMod/Core/Result.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace VoltMod
{

/** Which plugins the host loads and which it turns away, with the reason. Both alphabetical. */
struct LoadList
{
    std::vector<std::string> Allowed;
    std::vector<RefusedPlugin> Refused;
};

/** Pure: no files, logs or SDK. */
namespace PluginDependencies
{

/**
 * @brief Decide which of @p installed the host loads.
 *
 * A missing or refused required dependency refuses its dependents transitively; optional ones never
 * refuse. Load order is alphabetical and carries no meaning: plugins reach each other through
 * @ref VoltMod::ServiceExchange at call time.
 */
LoadList Resolve(std::span<const PluginManifest> installed);

/** The plugins in @p loaded that require @p plugin, directly or transitively; what `volt unload`
 *  refuses for and `volt reload` takes down with it. Alphabetical. */
std::vector<std::string> RequiredDependents(std::string_view plugin, std::span<const PluginManifest> loaded);

}  // namespace PluginDependencies
}  // namespace VoltMod

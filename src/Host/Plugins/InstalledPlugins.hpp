#pragma once

#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Host/PluginDescriptor.hpp>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace VoltMod
{

/** One parsed `plugin.json`. */
struct PluginManifest
{
    std::string Name;
    std::string Version;
    std::string LogTag;  ///< prefixes the plugin's log lines; its name unless the file says otherwise
    std::string Description;
    std::string Author;
    std::string Website;
    std::string License;
    /** The level the plugin starts at; `volt log` changes it until the next load. */
    VoltMod::LogLevel LogLevel = VoltMod::LogLevel::Info;
    std::vector<std::string> Dependencies;  ///< Required: a missing one refuses this plugin.
    /** The plugin loads without these and reaches them through the service exchange when present. */
    std::vector<std::string> OptionalDependencies;
};

/** A plugin the host will not load, and the reason its log line gives. */
struct RefusedPlugin
{
    std::string Name;
    Error Reason;
};

/** What the plugins directory holds: the manifests that parsed, and the ones that did not. */
struct InstalledScan
{
    std::vector<PluginManifest> Plugins;
    std::vector<RefusedPlugin> Refused;
};

/** SDK-free, so tests run it against a real plugins directory. */
namespace InstalledPlugins
{

/** Every `<plugins>/<name>/plugin.json`. A directory without one is skipped silently; a malformed
 *  manifest, or one naming another plugin, is refused. */
InstalledScan Discover(const std::filesystem::path& plugins);

}  // namespace InstalledPlugins

/** Whether @p descriptor is one a host of @p hostVersion can load, with the reason it is not. */
Status ValidateDescriptor(const PluginDescriptor* descriptor, std::string_view hostVersion);

}  // namespace VoltMod

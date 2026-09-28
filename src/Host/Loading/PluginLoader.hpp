#pragma once

#include "Host/Loading/InstalledPlugins.hpp"
#include "Host/Loading/SharedLibrary.hpp"
#include "Host/Plugins/PluginHost.hpp"

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Engine/EngineTypes.hpp>
#include <VoltMod/Host/PluginDescriptor.hpp>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace VoltMod
{

/** One plugin whose library is open and whose Load returned true. */
struct LoadedPlugin
{
    PluginManifest Manifest;
    const PluginDescriptor* Descriptor = nullptr;
    std::string Assets;  ///< the mounted `server-assets` folder; empty when none is
    SharedLibrary Code;  ///< last member: freed only after Unload has returned
};

/**
 * @brief Finds the installed plugins and loads the ones their dependencies allow.
 *
 * A load or unload asked for from the console is deferred, never acted on where it was asked: the
 * request arrives inside the console-command dispatch, and freeing a library there would pull the
 * ground out from under the call in flight. @ref RunPending runs them at the start of the next frame.
 */
class PluginLoader
{
public:
    enum class ActionKind
    {
        Load,
        Unload,
        Reload
    };

    explicit PluginLoader(PluginHost& host);
    ~PluginLoader();

    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;

    /** Load every installed plugin whose required dependencies are there. */
    void LoadAll();

    /** Unload every loaded plugin, newest first. */
    void UnloadAll();

    /** Take @p kind on @p name at the start of the next frame. */
    void Defer(ActionKind kind, std::string_view name);

    /** Act on what was deferred. The frame hook calls this before the frame reaches plugins. */
    void RunPending();

    /** In load order; unload runs it backwards. */
    const std::vector<LoadedPlugin>& LoadedPlugins() const { return _loaded; }

    /** The loaded plugin @p name, or nullptr after logging that it is not loaded. */
    LoadedPlugin* RequireLoaded(std::string_view name);

    /** Each plugin the last load attempt refused, by name, with the reason; a later load clears it. */
    const std::map<std::string, std::string>& Refused() const { return _refused; }

private:
    struct PendingAction
    {
        ActionKind Kind = ActionKind::Load;
        std::string Name;
    };

    /** Open the library, check its descriptor and give it a view of the host. */
    Status LoadOne(const PluginManifest& manifest);
    /** Players are disconnected from a plugin only mid-map: at shutdown the engine may be past
     *  running handlers. */
    enum class UnloadTime
    {
        MidMap,
        Shutdown,
    };

    /** Unload @p name, report whatever it left behind, then free its library. */
    void UnloadOne(std::string_view name, UnloadTime when);

    /** Put @p name's `server-assets` folder on the game's search path ahead of the game's own VPKs,
     *  which a loose file under `game/csgo` loses to. Weapon subclasses in it count from the next map
     *  load, so a plugin loaded at startup has them on the first map. Returns the mounted folder; empty
     *  when none is. */
    std::string MountAssets(std::string_view name);
    void UnmountAssets(const std::string& folder);

    /** Plan @p installed, log what it refuses, and load whatever @p wanted accepts. */
    void LoadGroup(const Discovered& installed, const std::function<bool(std::string_view)>& wanted);
    /** Log that @p name was refused and remember why, for `volt list`. */
    void Refuse(std::string_view name, std::string reason);

    LoadedPlugin* FindLoaded(std::string_view name);
    std::vector<PluginManifest> LoadedManifests() const;

    void RunLoad(std::string_view name);
    void RunUnload(std::string_view name);
    void RunReload(std::string_view name);

    PluginHost& _host;
    IFileSystem* _files = nullptr;  ///< null when the engine has none: no plugin's assets are mounted
    std::vector<LoadedPlugin> _loaded;
    std::vector<PendingAction> _pending;
    std::map<std::string, std::string> _refused;
};

}  // namespace VoltMod

#include "BuildStamp.hpp"
#include "Engine/Memory/ScriptBindings.hpp"
#include "Engine/Server/ConsoleLogger.hpp"
#include "Host/Console/VoltCommand.hpp"
#include "Host/EngineHooks.hpp"
#include "Host/EngineInterfaces.hpp"
#include "Host/Files/ServerAssets.hpp"
#include "Host/GameData/GameDataTable.hpp"
#include "Host/LoaderHandoff.hpp"
#include "Host/Plugins/PluginLoader.hpp"
#include "Host/Plugins/PluginRegistry.hpp"
#include "Host/Schema/SchemaCheck.hpp"
#include "Host/Workshop/WorkshopDownloads.hpp"

#include <VoltMod/Core/Files/Paths.hpp>
#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Core/Text/Strings.hpp>
#include <VoltMod/Engine/Detours.hpp>
#include <VoltMod/Host/PluginDescriptor.hpp>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string_view>

namespace KHook
{
KHook::IKHook* __exported__khook = nullptr;
}

namespace VoltMod
{

static constexpr std::string_view GameDataPath = "addons/voltmod/gamedata/gamedata.jsonc";

/** The host's services for the process, built at start and torn down at stop. */
class Host
{
public:
    ~Host() { Stop(); }

    Status Start(const LoaderHandoff& start)
    {
        KHook::__exported__khook = start.HookDispatcher;
        Log::SetHandler(MakeConsoleHandler("VoltMod"));
        SetBaseDir(start.GameDir);

        // Before any plugin, so a broken signature logs once; KHook reads slots another hook holds.
        _gameData = std::make_unique<GameDataTable>(GameDataPath, ReadOriginalSlot, FindScriptBinding);

        Result<EngineInterfaces> engine = ResolveEngineInterfaces(start);
        if (!engine)
        {
            Stop();
            return std::unexpected(engine.error());
        }

        GameDataTable* gameData = _gameData->Ready() ? _gameData.get() : nullptr;
        _downloads = std::make_unique<WorkshopDownloads>(gameData, *engine);
        _host = std::make_unique<PluginRegistry>(start, gameData, _downloads.get());
        // Once per process: every plugin built with this host carries the same baked offsets.
        _schema = std::make_unique<SchemaCheck>(*_host, engine->Schema, engine->Resources);
        _assets = std::make_unique<ServerAssets>(engine->Files);
        _plugins = std::make_unique<PluginLoader>(*_host, *_assets);
        _hooks = std::make_unique<EngineHooks>(
            *_host, *engine,
            [this] {
                _plugins->RunPending();
                _downloads->OnFrame();
            },
            [this] { _schema->OnServerStartup(); },
            [this](int64_t steamId) { _downloads->OnClientConnected(steamId); });

        // Before the plugins load, so a plugin registering `volt` is refused rather than racing it.
        _command = std::make_unique<VoltCommand>(*_host, *_plugins);
        _plugins->LoadAll();

        Log::Info("VoltMod host {} ({}) loaded.", VOLTMOD_VERSION, VOLTMOD_BUILD_STAMP);
        return {};
    }

    void Stop()
    {
        // Plugins first: their teardown runs while the host's events and services are still there.
        if (_plugins)
        {
            _plugins->UnloadAll();
        }

        _hooks.reset();
        _command.reset();
        _plugins.reset();
        _assets.reset();
        _schema.reset();
        _host.reset();
        _downloads.reset();
        _gameData.reset();
    }

private:
    std::unique_ptr<GameDataTable> _gameData;
    std::unique_ptr<WorkshopDownloads> _downloads;
    std::unique_ptr<PluginRegistry> _host;
    std::unique_ptr<SchemaCheck> _schema;
    std::unique_ptr<ServerAssets> _assets;
    std::unique_ptr<PluginLoader> _plugins;
    std::unique_ptr<VoltCommand> _command;
    std::unique_ptr<EngineHooks> _hooks;
};

static Host g_host;

}  // namespace VoltMod

extern "C" VOLTMOD_EXPORT bool VoltMod_HostStart(const VoltMod::LoaderHandoff* start, char* error, size_t errorSize)
{
    VoltMod::Status started = VoltMod::g_host.Start(*start);
    if (!started)
    {
        VoltMod::Strings::CopyToBuffer(error, errorSize, started.error().Detail);
    }
    return started.has_value();
}

extern "C" VOLTMOD_EXPORT void VoltMod_HostStop()
{
    VoltMod::g_host.Stop();
}

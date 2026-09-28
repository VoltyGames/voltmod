#pragma once

#include "Host/EngineInterfaces.hpp"
#include "Host/GameData/GameDataTable.hpp"
#include "Host/Workshop/DownloadQueue.hpp"

#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <VoltMod/Host/IPluginAddons.hpp>
#include <cstdint>
#include <map>
#include <vector>

namespace VoltMod
{

/**
 * The workshop addons connecting clients must download, for every plugin at once: one set of hooks
 * and one list, so an addon two plugins require downloads once. One addon per reconnect; see
 * @ref workshop_guide. Game thread only.
 */
class WorkshopDownloads final : public IPluginAddons
{
public:
    /** Null @p gameData or a listen server downloads nothing. */
    WorkshopDownloads(GameDataTable* gameData, const EngineInterfaces& engine);

    /** Require @p addonId of every client until @ref Release; 0 when nothing was added. */
    uint64_t Add(uint64_t addonId) override;
    void Release(uint64_t token) override;

    /** Whether @p slot has every required addon. */
    bool IsReady(int slot) override;

    /** A reconnect is the only sign a download finished. */
    void OnClientConnected(int64_t steamId);

    /** Runs the kicks queued last frame: kicking inside the send hook crashes on Windows. */
    void OnFrame();

private:
    Status InstallHooks();
    void RemoveHooksIfUnused();

    void OnJoinMessage(const CNetMessage* message, void* client);
    /** Add @p client's addons to the server's list for its connection reply, then remove them. */
    void AddToReply(CNetworkGameServerBase& server, const EngineClient* client);
    void RestoreReply(CNetworkGameServerBase& server);

    struct Kick
    {
        int Slot = -1;
        int64_t SteamId = 0;
    };

    IVEngineServer2* _engine;
    Bindings _bindings;
    bool _bound = false;
    DownloadQueue _queue;
    std::map<uint64_t, uint64_t> _tokens;  ///< token -> addon
    uint64_t _nextToken = 1;
    std::vector<Kick> _kicks;
    std::vector<uint64_t> _addedToReply;
    Subscription _joinMessageHook;      ///< tells a client what to download
    Subscription _connectionReplyHook;  ///< tells a client what to mount
};

}  // namespace VoltMod

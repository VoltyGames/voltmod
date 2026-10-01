#pragma once

#include "Engine/Net/ProtoReflect.hpp"
#include "Host/EngineInterfaces.hpp"
#include "Host/GameData/GameDataTable.hpp"
#include "Host/Plugins/PluginRegistry.hpp"

#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Slots/Slot.hpp>
#include <VoltMod/Engine/GameData/Bindings.hpp>
#include <array>
#include <string>
#include <vector>

namespace VoltMod
{

/**
 * @brief The FilterMessage hook for every plugin: custom HUD button presses, raised on the frame
 * after they arrive. Game thread only.
 */
class ButtonPresses
{
public:
    /** Null @p gameData or a missing message registry hooks nothing. @p registry must outlive this. */
    ButtonPresses(GameDataTable* gameData, const EngineInterfaces& engine, PluginRegistry& registry);

    ButtonPresses(const ButtonPresses&) = delete;
    ButtonPresses& operator=(const ButtonPresses&) = delete;

    /** Raise the presses that arrived since the last frame. */
    void OnFrame();

private:
    struct Press
    {
        int Slot = -1;
        std::string ButtonId;
    };

    /** The user message fields a press is read from, resolved on the first message. */
    struct MessageFields
    {
        const ProtoFieldDescriptor* Type = nullptr;
        const ProtoFieldDescriptor* Data = nullptr;

        explicit operator bool() const { return Type && Data; }
    };

    static const MessageFields& FieldsOf(const ProtoMessage& proto);

    void Queue(const CNetMessage* message, const INetworkMessageProcessingPreFilter& filter);
    void WarnMalformed(int slot, std::string_view detail);

    PluginRegistry& _registry;
    Bindings _bindings;
    int _messageId = -1;
    std::vector<Press> _queued;
    std::array<double, MaxPlayers> _lastWarning{};  ///< a client can send malformed presses at will
    Subscription _hook;
};

}  // namespace VoltMod

#pragma once

#include <VoltMod/Core/Signals/Subscription.hpp>
#include <VoltMod/Core/Text/Translations.hpp>
#include <VoltMod/Hooks/ClientConVars.hpp>
#include <VoltMod/Players/PlayerManager.hpp>

namespace VoltMod::Internal
{

/** Gives a player with no language yet the one their Steam client runs in (`cl_language`). The
 *  language lives in the host, so the first plugin to hear back sets it for all; a pick made in a
 *  plugin's menu still wins. */
class ClientLanguage
{
public:
    ClientLanguage(PlayerManager& players, ClientConVars& clientConVars, Translations& translations);

private:
    ClientConVars& _clientConVars;
    Translations& _translations;
    /** Declared last so the handler stops before the state it captures. */
    Subscription _connected;
};

}  // namespace VoltMod::Internal

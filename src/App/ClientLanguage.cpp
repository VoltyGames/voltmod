#include <VoltMod/App/Internal/ClientLanguage.hpp>
#include <VoltMod/Players/Player.hpp>
#include <array>
#include <string_view>

namespace VoltMod::Internal
{

struct SteamLanguage
{
    std::string_view Steam;
    std::string_view Code;
};

/** Steam's language names; a code no plugin translates falls back to the server language. */
static constexpr std::array SteamLanguages{
    SteamLanguage{.Steam = "english", .Code = "en"},    SteamLanguage{.Steam = "russian", .Code = "ru"},
    SteamLanguage{.Steam = "ukrainian", .Code = "uk"},  SteamLanguage{.Steam = "german", .Code = "de"},
    SteamLanguage{.Steam = "french", .Code = "fr"},     SteamLanguage{.Steam = "spanish", .Code = "es"},
    SteamLanguage{.Steam = "portuguese", .Code = "pt"}, SteamLanguage{.Steam = "polish", .Code = "pl"},
    SteamLanguage{.Steam = "turkish", .Code = "tr"},
};

static std::string_view CodeFor(std::string_view steamLanguage)
{
    for (const SteamLanguage& language : SteamLanguages)
    {
        if (language.Steam == steamLanguage)
        {
            return language.Code;
        }
    }
    return {};
}

ClientLanguage::ClientLanguage(PlayerManager& players, ClientConVars& clientConVars, Translations& translations)
    : _clientConVars(clientConVars), _translations(translations)
{
    _connected = players.FullyConnected += [this](Player& player) {
        if (player.IsBot() || !_translations.PlayerLanguage(player.Slot()).empty())
        {
            return;
        }
        // Pending queries are dropped when the slot changes hands, so the answer is this player's.
        _clientConVars.Query(player.Slot(), "cl_language",
                             [this](int slot, ClientConVarStatus status, std::string_view, std::string_view value) {
                                 const std::string_view code = CodeFor(value);
                                 // A pick made while the query was out wins.
                                 const bool unset = _translations.PlayerLanguage(slot).empty();
                                 if (status == ClientConVarStatus::Answered && !code.empty() && unset)
                                 {
                                     _translations.SetPlayerLanguage(slot, code);
                                 }
                             });
    };
}

}  // namespace VoltMod::Internal

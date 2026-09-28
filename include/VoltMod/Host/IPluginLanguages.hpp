#pragma once

#include <string_view>

namespace VoltMod
{

/** Each player's language, one table for every plugin. The host clears a slot when it changes hands. */
struct IPluginLanguages
{
    /** Empty when no plugin has set one. */
    virtual std::string_view Language(int slot) const = 0;
    /** Empty clears it. */
    virtual void SetLanguage(int slot, std::string_view lang) = 0;

protected:
    ~IPluginLanguages() = default;
};

}  // namespace VoltMod

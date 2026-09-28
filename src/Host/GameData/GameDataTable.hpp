#pragma once

#include "Host/GameData/GameDataDocument.hpp"
#include "Host/GameData/GameDataScanner.hpp"

#include <VoltMod/Engine/Memory/OriginalSlotLookup.hpp>
#include <VoltMod/Host/IPluginGameData.hpp>
#include <memory>
#include <string>
#include <string_view>

namespace VoltMod
{

/** The process's one gamedata resolution, made at host load and served to every plugin. What
 *  did not bind is logged here once; each plugin's services report it again as unavailable. */
class GameDataTable final : public IPluginGameData
{
public:
    /** Read and resolve @p path. @p originalOf reads a slot another module already hooked. */
    explicit GameDataTable(std::string_view path, const OriginalSlotLookup& originalOf = {},
                           const ScriptBindingLookup& scriptOf = {});

    /** False when the file could not be read. */
    bool Ready() const { return _scanner != nullptr; }

    GameDataLocation Lookup(GameDataSection sections, std::string_view key) override;

private:
    /** An entry that did not bind, keeping @p reason until the next lookup. */
    GameDataLocation NotBound(std::string reason);

    // The scanner keeps a reference to the document, so the two live and die together.
    std::unique_ptr<GameDataDocument> _file;
    std::unique_ptr<GameDataScanner> _scanner;
    OriginalSlotLookup _originalOf;
    ScriptBindingLookup _scriptOf;
    std::string _reason;
};

}  // namespace VoltMod

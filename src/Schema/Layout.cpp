#include "Schema/Layout.hpp"

#include <format>
#include <string>
#include <vector>

namespace VoltMod::Schema
{

Status ResolveLayout(std::span<FieldLayout> fields, const LiveFieldLookup& lookup)
{
    // Report all mismatches so one changed class does not hide the rest.
    std::vector<std::string> broken;
    for (FieldLayout& field : fields)
    {
        const LiveField live = lookup(field.Class, field.Field);
        field.Offset = live.Offset;
        if (live.Offset < 0)
        {
            broken.push_back(std::format("{}::{}: gone", field.Class, field.Field));
            continue;
        }

        if (field.Size != 0 && live.Size != 0 && live.Size != field.Size)
        {
            broken.push_back(std::format("{}::{}: size {} -> {}", field.Class, field.Field, field.Size, live.Size));
        }
    }

    if (broken.empty())
    {
        return {};
    }

    std::string message =
        "the server schema no longer matches these fields; regenerate with voltmod framework schemagen:";
    for (const std::string& line : broken)
    {
        message += std::format("\n  {}", line);
    }
    return std::unexpected(Error::Invalid(message));
}

}  // namespace VoltMod::Schema

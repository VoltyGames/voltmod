#include "Schema/Layout.hpp"

#include <doctest/doctest.h>
#include <string>
#include <string_view>

using VoltMod::Schema::FieldLayout;
using VoltMod::Schema::LiveField;
using VoltMod::Schema::ResolveLayout;

/** The live schema of a server where m_iHealth moved, m_flGone was removed and m_vecSize grew. */
static LiveField Live(std::string_view className, std::string_view field)
{
    if (className != "CBaseEntity")
    {
        return {};
    }
    if (field == "m_iHealth")
    {
        return {.Offset = 724, .Size = 4};
    }
    if (field == "m_vecSize")
    {
        return {.Offset = 900, .Size = 16};
    }
    if (field == "m_stats")
    {
        return {.Offset = 1000, .Size = 184};
    }
    if (field == "m_unsized")
    {
        return {.Offset = 1100, .Size = 0};
    }
    return {};
}

TEST_CASE("ResolveLayout takes each offset from the live schema")
{
    FieldLayout fields[] = {
        {.Class = "CBaseEntity", .Field = "m_iHealth", .Size = 4},
    };

    CHECK(ResolveLayout(fields, Live));
    CHECK(fields[0].Offset == 724);
}

TEST_CASE("ResolveLayout names every gone and resized field in one error")
{
    FieldLayout fields[] = {
        {.Class = "CBaseEntity", .Field = "m_iHealth", .Size = 4},
        {.Class = "CBaseEntity", .Field = "m_flGone", .Size = 4},
        {.Class = "CBaseEntity", .Field = "m_vecSize", .Size = 12},
    };

    const VoltMod::Status resolved = ResolveLayout(fields, Live);

    REQUIRE_FALSE(resolved);
    const std::string& detail = resolved.error().Detail;
    CHECK(detail.find("CBaseEntity::m_flGone: gone") != std::string::npos);
    CHECK(detail.find("CBaseEntity::m_vecSize: size 12 -> 16") != std::string::npos);
    CHECK(detail.find("m_iHealth") == std::string::npos);
    CHECK(fields[0].Offset == 724);
    CHECK(fields[1].Offset == -1);
}

TEST_CASE("ResolveLayout skips the size check when either side has no size")
{
    FieldLayout fields[] = {
        {.Class = "CBaseEntity", .Field = "m_stats", .Size = 0},
        {.Class = "CBaseEntity", .Field = "m_unsized", .Size = 4},
    };

    CHECK(ResolveLayout(fields, Live));
    CHECK(fields[0].Offset == 1000);
    CHECK(fields[1].Offset == 1100);
}

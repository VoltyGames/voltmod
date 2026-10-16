#pragma once

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Schema/LiveField.hpp>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace VoltMod::Schema
{

/** Finds @p field on @p className or its bases in the live server schema. */
using LiveFieldLookup = std::function<LiveField(std::string_view className, std::string_view field)>;

/** One field the generated accessors use; its offset comes from the live schema at load. */
struct FieldLayout
{
    std::string_view Class;
    std::string_view Field;
    int32_t Size = 0;  ///< checked against the live field; 0 skips the check
    int32_t Offset = -1;
};

/** This module's own copy: the generated accessors read their offsets from it. */
extern FieldLayout GeneratedFields[];

/** Every field in @ref GeneratedFields, owner links included. */
std::span<FieldLayout> GeneratedLayout();

/**
 * Fill every offset in @p fields from @p lookup. Game thread, before any accessor runs.
 * @return Every gone or resized field in one message; offsets that were found are filled either way.
 */
Status ResolveLayout(std::span<FieldLayout> fields, const LiveFieldLookup& lookup);

}  // namespace VoltMod::Schema

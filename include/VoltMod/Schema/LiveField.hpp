#pragma once

#include <cstdint>

namespace VoltMod::Schema
{

/** Where the running server lays out one schema field: Offset -1 when it is gone, Size 0 when unknown. */
struct LiveField
{
    int32_t Offset = -1;
    int32_t Size = 0;
};

}  // namespace VoltMod::Schema

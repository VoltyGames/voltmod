#pragma once

#include <schemasystem/schematypes.h>
#include <string_view>

namespace VoltMod::Schema
{

/** The owner link field a replicated component carries. */
inline constexpr std::string_view ChainField = "__m_pChainEntity";

/**
 * Find @p field on @p klass or its first bases, most-derived first. Schema offsets flatten single
 * inheritance, so a base field's offset works on the derived object.
 */
inline const SchemaClassFieldData_t* FindField(const CSchemaClassInfo* klass, std::string_view field)
{
    for (; klass; klass = klass->m_nBaseClassCount > 0 ? klass->m_pBaseClasses[0].m_pClass : nullptr)
    {
        for (uint16_t i = 0; i < klass->m_nFieldCount; ++i)
        {
            const char* name = klass->m_pFields[i].m_pszName;
            if (name && field == name)
            {
                return &klass->m_pFields[i];
            }
        }
    }
    return nullptr;
}

}  // namespace VoltMod::Schema

#pragma once

#include <VoltMod/Core/Result.hpp>
#include <VoltMod/Database/Connection.hpp>
#include <VoltMod/Database/DatabaseConfig.hpp>
#include <VoltMod/Database/Driver.hpp>

namespace VoltMod
{

/** The driver @p config names, or why it cannot connect. */
Result<Driver> CheckConfig(const DatabaseConfig& config);

bool IsOpen(const AnyConnection& connection);

/** Opens a @p driver connection into @p connection. Throws what the driver throws, whose text can
 *  hold the password. */
void OpenConnection(AnyConnection& connection, Driver driver, const DatabaseConfig& config);

}  // namespace VoltMod

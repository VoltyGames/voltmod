#include "Database/OpenConnection.hpp"

#include <VoltMod/Core/Files/Paths.hpp>
#include <algorithm>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace VoltMod
{

using PostgresSslMode = sqlpp::postgresql::connection_config::sslmode_t;

struct SslModeName
{
    std::string_view Name;
    PostgresSslMode Mode;
};

static constexpr SslModeName SslModes[] = {
    {"disable", PostgresSslMode::disable},     {"allow", PostgresSslMode::allow},
    {"prefer", PostgresSslMode::prefer},       {"require", PostgresSslMode::require},
    {"verify-ca", PostgresSslMode::verify_ca}, {"verify-full", PostgresSslMode::verify_full},
};

/** The modes MariaDB, which takes a bool, reads as "the connection must use TLS". */
static constexpr PostgresSslMode TlsModes[] = {PostgresSslMode::require, PostgresSslMode::verify_ca,
                                               PostgresSslMode::verify_full};

static constexpr std::string_view InMemory = ":memory:";

static std::optional<PostgresSslMode> ParseSslMode(std::string_view name)
{
    const auto found = std::ranges::find(SslModes, name, &SslModeName::Name);
    return found != std::ranges::end(SslModes) ? std::optional(found->Mode) : std::nullopt;
}

static sqlpp::postgresql::connection_config PostgresSettings(const DatabaseConfig& config)
{
    sqlpp::postgresql::connection_config settings;
    settings.host = config.host;
    settings.port = config.port > 0 ? static_cast<uint32_t>(config.port) : 5432u;
    settings.dbname = config.database;
    settings.user = config.username;
    settings.password = config.password;
    settings.connect_timeout = static_cast<uint32_t>(config.connectTimeoutSec);
    settings.sslmode = ParseSslMode(config.sslMode).value_or(PostgresSslMode::prefer);
    return settings;
}

static sqlpp::mysql::connection_config MariaDbSettings(const DatabaseConfig& config)
{
    sqlpp::mysql::connection_config settings;
    settings.host = config.host;
    settings.port = config.port > 0 ? static_cast<unsigned int>(config.port) : 3306u;
    settings.database = config.database;
    settings.user = config.username;
    settings.password = config.password;
    settings.connect_timeout_seconds = static_cast<unsigned int>(config.connectTimeoutSec);
    settings.charset = "utf8mb4";
    settings.ssl = std::ranges::contains(TlsModes, ParseSslMode(config.sslMode).value_or(PostgresSslMode::prefer));
    // Affected rows must mean matched rows, which update-then-insert reads to decide on the insert.
    settings.client_flag |= CLIENT_FOUND_ROWS;
    return settings;
}

static sqlpp::sqlite3::connection_config SqliteSettings(const DatabaseConfig& config)
{
    sqlpp::sqlite3::connection_config settings;
    settings.flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    if (config.path == InMemory)
    {
        settings.path_to_database = config.path;
        return settings;
    }

    // Relative paths must resolve against the game dir, not the server process cwd.
    const std::filesystem::path file = ResolvePath(config.path);
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    settings.path_to_database = file.string();
    return settings;
}

Result<Driver> CheckConfig(const DatabaseConfig& config)
{
    const std::optional<Driver> driver = ParseDriver(config.driver);
    if (!driver)
    {
        return std::unexpected(Error::Invalid(
            std::format("Unknown database driver '{}'; expected postgres, mariadb or sqlite.", config.driver)));
    }
    if (*driver == Driver::Sqlite && config.path.empty())
    {
        return std::unexpected(Error::Invalid("Database driver 'sqlite' needs a 'path' (a file, or \":memory:\")."));
    }
    if (*driver == Driver::Postgres && !ParseSslMode(config.sslMode))
    {
        return std::unexpected(Error::Invalid(std::format(
            "Unknown sslMode '{}'; expected disable, allow, prefer, require, verify-ca or verify-full.", config.sslMode)));
    }
    return *driver;
}

bool IsOpen(const AnyConnection& connection)
{
    return std::visit(
        [](const auto& conn) {
            if constexpr (std::same_as<std::remove_cvref_t<decltype(conn)>, std::monostate>)
            {
                return false;
            }
            else
            {
                return conn.is_connected();
            }
        },
        connection);
}

void OpenConnection(AnyConnection& connection, Driver driver, const DatabaseConfig& config)
{
    switch (driver)
    {
    case Driver::Postgres:
        connection.emplace<PostgresConnection>(PostgresSettings(config));
        return;
    case Driver::MariaDb:
        sqlpp::mysql::global_library_init();
        connection.emplace<MariaDbConnection>(MariaDbSettings(config));
        return;
    case Driver::Sqlite:
    {
        auto& conn = connection.emplace<SqliteConnection>(SqliteSettings(config));
        if (config.path != InMemory)
        {
            conn("PRAGMA journal_mode=WAL");
        }
        conn("PRAGMA busy_timeout=" + std::to_string(config.connectTimeoutSec * 1000));
        return;
    }
    }
}

}  // namespace VoltMod

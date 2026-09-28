#include <VoltMod/Core/LoadReport.hpp>
#include <doctest/doctest.h>
#include <string>
#include <utility>

using VoltMod::Error;
using VoltMod::LoadReport;
using VoltMod::Status;

static Status Fails(std::string reason)
{
    return std::unexpected(Error::Failed(std::move(reason)));
}

TEST_CASE("LoadReport::Optional keeps loading and remembers the failure")
{
    LoadReport report;
    CHECK(report.Optional("Database", {}));
    CHECK_FALSE(report.Optional("Admins", Fails("no rows")));

    REQUIRE_EQ(report.Failures().size(), 1u);
    CHECK_EQ(report.Failures()[0].Name, "Admins");
    CHECK_EQ(report.Failures()[0].Reason, "no rows");
    CHECK_FALSE(report.Failures()[0].Required);
    CHECK(report.AbortReason().empty());
}

TEST_CASE("LoadReport::AbortReason names the first required check that failed")
{
    LoadReport report;
    report.Optional("Database", Fails("offline"));
    CHECK_FALSE(report.Required("Configuration", Fails("settings.jsonc: unknown key")));
    report.Required("SchemaLayout", Fails("drift"));

    CHECK_EQ(report.AbortReason(), "Configuration: settings.jsonc: unknown key");
}

TEST_CASE("LoadReport::Summary lists only the checks that failed")
{
    LoadReport report;
    report.Optional("Entities", {});
    CHECK(report.Summary().find("failed") == std::string::npos);

    report.Optional("Database", Fails("offline"));
    const std::string summary = report.Summary();
    CHECK(summary.starts_with("Load took "));
    CHECK(summary.find("Entities") == std::string::npos);
    CHECK(summary.find("optional Database: offline") != std::string::npos);
}

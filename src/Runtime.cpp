#include <VoltMod/Core/Files/Paths.hpp>
#include <VoltMod/Core/Log.hpp>
#include <VoltMod/Core/Text/Json.hpp>
#include <VoltMod/Menu/PanoramaMenu.hpp>
#include <VoltMod/Players/Permissions.hpp>
#include <VoltMod/Runtime.hpp>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace VoltMod
{

Runtime::Runtime(IPluginContext& host, UnsafeServices& unsafe, PlayerLanguages& languages)
    : PluginName(host.Name()),
      Version(host.Version()),
      Translations(languages),
      Unsafe(unsafe),
      AddonManager(host.Addons()),
      Exchange(host.Services()),
      Commands(Policy, Translations, Players, Entities, Messages, host)
{
    Policy.HasPermission = [this, warned = false](int64_t steamId, std::string_view permission) mutable {
        if (auto* permissions = Exchange.Get<IPermissions>())
        {
            return permissions->HasPermission(steamId, permission);
        }
        if (!warned)
        {
            warned = true;
            Log::Warn("Denying '{}': no plugin publishes {}.", permission, IPermissions::InterfaceName);
        }
        return false;
    };

    CheckServices();
    AddStatusSections();
}

Runtime::~Runtime()
{
    // Stop HTTP workers and flush queued logs before removing frame hooks.
    Http.Stop();
    Log::DeliverPending();
}

std::string Runtime::PluginFile(std::string_view relative) const
{
    return VoltMod::PluginFile(PluginName, relative);
}

Subscription Runtime::UsePanorama(PanoramaMenuLayout& layout)
{
    auto menu = std::make_unique<PanoramaMenu>(PanoramaMenu::Services{.Scheduler = Scheduler,
                                                                      .Slots = Slots,
                                                                      .Freeze = Freeze,
                                                                      .ChatInput = ChatInput,
                                                                      .Translations = Translations,
                                                                      .Policy = Policy,
                                                                      .Screens = Screens,
                                                                      .AddonManager = AddonManager},
                                               layout);
    Subscription preferred = Menus.Prefer(*menu);
    // Stop routing to the menu before destroying it.
    return Subscription([menu = std::move(menu), preferred = std::move(preferred)]() mutable {
        preferred.Reset();
        menu.reset();
    });
}

void Runtime::CheckServices()
{
    LoadReport.Required("Messages", Messages.Available());
    LoadReport.Optional("GameData", Unsafe.GameData);
    LoadReport.Optional("Entities", Entities.Available());
    LoadReport.Optional("ConVars", ConVars.Available());
    LoadReport.Optional("GameEvents", GameEvents.Available());
    LoadReport.Optional("ClientConVars", ClientConVars.Available());
    LoadReport.Optional("Movement", Movement.Available());
    LoadReport.Optional("Teleport", Teleport.Available());
    LoadReport.Optional("Visibility", Visibility.Available());
    LoadReport.Optional("Trace", Trace.Available());
    LoadReport.Optional("Screens", Screens.Available());
    LoadReport.Optional("Damage", Damage.Available());
}

void Runtime::AddStatusSections()
{
    // Plugins add status sections during Load; the runtime owns them for the load cycle.
    Status.RegisterSection("build", [this] { return Json::Write(glz::obj{"name", PluginName, "version", Version}); });

    Status.RegisterSection("load", [this] {
        std::map<std::string, std::string> failed;
        for (const FailedCheck& check : LoadReport.Failures())
        {
            failed.emplace(check.Name, check.Reason);
        }
        return Json::Write(glz::obj{"failed", failed});
    });

    Status.RegisterSection("uptime", [start = std::chrono::steady_clock::now()] {
        const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start);
        return Json::Write(glz::obj{"seconds", uptime.count()});
    });
}

void Runtime::OnGameFrame()
{
    Log::DeliverPending();
    Scheduler.OnGameFrame();
}

}  // namespace VoltMod

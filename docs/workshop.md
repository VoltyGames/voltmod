# Workshop addons {#workshop_guide}

[TOC]

@ref VoltMod::MultiAddonManager makes connecting clients download Steam Workshop content: Panorama
layouts, models, sounds.

```cpp
#include <VoltMod/Workshop/MultiAddonManager.hpp>

// App::Load; the Subscription requires the addon until it drops
_subs.Add(runtime.AddonManager.Add(Config.Get().addonId));

bool ready = runtime.AddonManager.IsReady(slot);   // this player has every addon any plugin requires
```

`Add` requires the addon of every connecting client until the returned `Subscription` drops, so
keep it beside the feature that needs the content. Id 0 requires nothing, without an error or a
log line, which lets a plugin read the id from its settings and default it to 0. `IsReady(slot)`
tells whether a player has every addon any plugin requires; a player who is not ready gets center
HTML instead of a Panorama menu.

Requirements take effect on a client's next connect; already-connected players are not disturbed.

## Building the screens

`voltmod panorama compile [PLUGIN...] --addon NAME --no-deploy` compiles the screens into
`game/csgo_addons/NAME/`, the folder the Workshop Manager uploads. See @ref panorama_guide_publish.

## One list for all plugins

The host keeps one list of required addons for every plugin, with one set of engine hooks, so an
addon several plugins require downloads once. A requirement a plugin still holds when it unloads
is released and reported in the host's leak warnings.

## One addon per reconnect

Two engine messages carry an addon and a client needs both:

- The join message (`CNETMsg_SignonState`) sends the client away to download an addon and
  reconnect. The host rewrites it with the next missing addon.
- The connection reply (`CNetworkGameServer::ReplyConnection`) names the addons the client mounts
  for the session. The reply copies the server's own addon list, so the host appends this client's
  downloaded addons plus the one being downloaded, then removes its additions.

A client that downloaded an addon but was not told to mount it has the files and no content: a
menu drawn on that layout is invisible.

The join message's field is a comma-separated list, but a client handles exactly one addon per
connection cycle and stalls without downloading when it receives several. The host reduces such a
message to its first addon, so the client makes progress. Each addon therefore costs the joining
client one reconnect, including the first.

The server gets no download-complete signal. A reconnect within 30 seconds counts as success; a
later one retries the addon. The same addon is offered at most 3 times before a declining client
is dropped, which stops an endless reconnect loop. Progress is keyed by SteamID, because a client
cycling through downloads changes slots.

## Building models and effects

A plugin's workshop sources live in its `content/` folder. Compile one folder of it with the CS2
Workshop Tools, optionally copying the result loose into a local client or server:

```bash
voltmod content compile <plugin> models/<plugin>/crate --install client --install server
```

It mirrors the folder into `content/csgo_addons/<plugin>/` of the client (`--addon` picks another
addon name), compiles every `.vmdl`, `.vpcf`, `.vsndevts` and `.vdata` in it, and `--prune` deletes
source files nothing references. Publish the compiled addon from the Workshop Manager.

## Server-side content

The server does not download workshop addons. Put the compiled files the server itself needs -
models, particles, sound events, `scripts/weapons.vdata_c` - in the plugin's `server-assets/`
folder; `voltmod content server-assets <plugin>` refreshes it from the compiled addon, leaving out
any file whose source is gone from `content/`. It installs with the plugin, and while the plugin is
loaded the host mounts it ahead of the game's own VPKs. The game reads weapon subclasses when a map loads, so they apply from the next
map. See @ref plugin_guide for the installed layout. A workshop map still goes through
@ref VoltMod::Map::ChangeToWorkshop.

## Availability

Nothing is sent on a listen server, where there is no download step, or when the
`CServerSideClient::SendNetMessage` vtable entry, the `CNetworkGameServer::ReplyConnection`
signature, or the client and server offsets they read did not bind. Either way `Add` logs a
warning in the host and requires nothing.

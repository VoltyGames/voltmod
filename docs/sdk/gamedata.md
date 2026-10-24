# Gamedata and schema {#sdk_gamedata_guide}

[TOC]

Gamedata says where engine code and untyped fields live. The schema says where entity fields live.
Both are resolved once and read from there.

| | says where | source | checked |
| --- | --- | --- | --- |
| `gamedata/gamedata.jsonc` | functions, globals, vtable slots, byte offsets | hand-maintained | by the host at startup, per entry |
| `gamedata/schema/manifest.json` | which entity fields get accessors | hand-maintained | offsets and sizes taken from the live schema at load |

## Bindings

The host reads `addons/voltmod/gamedata/gamedata.jsonc` and scans for every entry once, before any
plugin loads. @ref VoltMod::Bindings gives those locations C++ types:

```cpp
// include/VoltMod/Engine/GameData/Bindings.hpp
Fn<CEntityInstance*(const char*, int)> CreateEntityByName;   // functions."CreateEntityByName"
Address GameEventManager;                                    // globals."CSource2Server::g_GameEventManager"
VirtualFn<void(CEntityInstance*, int)> ChangeTeam;           // vtables."CCSPlayerController::ChangeTeam"
Offset<void> UserCmdProto;                                   // offsets."CUserCmd::CSGOUserCmdPB"
```

Each plugin module calls `Bindings::Bind` before it builds the runtime, taking every member in one pass from
a @ref VoltMod::GameDataLookup over what the host already resolved. Services then hold
`const Bindings&`, so no call path does a string lookup. No plugin reads the file or scans memory
itself: a signature a game update broke costs one scan and one error line for the whole server.

`Bindings::Failures` lists `key: reason` for every member the last `Bind` left unbound.

## gamedata.jsonc format

Four sections, named for what they bind. A key is the engine's own name for the symbol, or
`Class::member`, so it can be checked against upstream gamedata and against the binary. Each key
belongs to exactly one section.

```jsonc
{
  "$schema": "https://raw.githubusercontent.com/VoltyGames/voltmod/main/gamedata/gamedata.schema.json",
  // steam.inf ServerVersion the entries were last checked on, and the date.
  "build": { "server": "2000908", "verified": "2026-09-11" },

  // A byte pattern matching the start of a function, or the function a VScript binding calls.
  "functions": {
    "CreateEntityByName": {
      "module": "server",                                  // default; "engine2" for engine code
      "windows": "48 83 EC 48 C6 44 24 30 00",
      "linux": "48 8D 05 ? ? ? ? 55 48 89 FA"
    },
    "CBaseEntity::EmitSoundParams": { "class": "CBaseEntity", "script": "EmitSoundParams" }
  },

  // A global reached through the rel32 displacement `rel32At` bytes after the match.
  "globals": {
    "CSource2Server::g_GameEventManager": {
      "windows": { "pattern": "48 8B 0D ? ? ? ? E8 ? ? ? ? 45 33 FF 4C 39 7D 50", "rel32At": 3 },
      "linux": { "pattern": "48 8D 05 ? ? ? ? 48 8B 38 E8 ? ? ? ? 49 83 7D 50 00", "rel32At": 3 }
    }
  },

  // A vtable slot, counted in the primary table of `class`, or in the table of its base `base`,
  // or the slot a virtual VScript binding dispatches through, or a class table alone.
  "vtables": {
    "CPlayer_MovementServices::RunCommand": { "class": "CCSPlayer_MovementServices", "windows": 25, "linux": 26 },
    "CCSPlayerController::ChangeTeam": { "class": "CCSPlayerController", "script": "SetTeam" },
    "CServerSideClient": { "class": "CServerSideClient", "module": "engine2" }
  },

  // A byte offset into a layout the SDK headers do not declare, or where a base sits in a class.
  "offsets": {
    "CUserCmd::CSGOUserCmdPB": { "windows": 16, "linux": 16 }
  }
}
```

Wildcard bytes are `?` or `??`. A `class` name is the top-level RTTI name, with no namespace or
template. An offset entry is either per-platform numbers or a `class` + `base` pair read from RTTI
at load, never both.

A `script` entry has no platform columns. The host reads `class`'s VScript description, searches
its bindings and then its bases' for the name, and takes the function, or for a vtable entry the
slot the binding dispatches through. A VScript binding can vanish in an update, so use one only
where the binding calls the engine function itself.

A `vtables` entry with neither platform columns nor `script` binds a `ClassTable`: the class table
alone. Use it where the SDK declares the virtual. `HookTable` takes the slot from that declaration,
so a game update needs no index check for it.

`gamedata.schema.json` in the framework repository has `additionalProperties: false` everywhere,
and the file's `$schema` loads it from GitHub, so an editor flags a typo before the server sees it.
Only `gamedata.jsonc` ships; the schema is never copied to a server or into the package.

## What the host checks

The file is read strictly: an unknown key, a wrong value type, or a section that is not an object
rejects the whole file, and the host then has nothing to serve.

Otherwise every entry is resolved and each failure is logged once with its reason:

- the key is missing, is in two sections, or is in a section that member does not bind from;
- the entry has no column for this platform;
- a pattern is empty, its module is not loaded, or it matches nowhere or more than once;
- a global's rel32 displacement, or the address it points at, falls outside its module;
- an offset or index is negative;
- a vtable's class table is not found, or its slot does not hold code;
- a `script` binding is not in the class or its bases, or is virtual where a function is wanted,
  or not virtual where a slot is;
- a `base` is not in its class through RTTI, is in it more than once, is virtual, or has no vtable
  of its own.

Each plugin's `GameData` load check then lists the failures touching its own members, and each
feature reports its own through `Available()`.

A pattern is validated by matching exactly once. A vtable index or a byte offset cannot be
validated that way, so when `build.server` does not match the running server the load warns that
those columns are unchecked on this build. The log records each vtable slot's code as
`key=module+offset`, to match a crash dump against a binding.

When every entry resolves, the host writes `addons/voltmod/gamedata/dumps/resolved.json`:
the server build, module-relative addresses for functions, globals and class tables, plus slot
indices and offsets. It is written once per server build, and a failed write does not fail the
load. Keep the record from a known-good build to diff against after an update.

## Schema fields

Schema offsets need no gamedata. `voltmod framework schemagen` generates typed accessors, and each
module (the host and every plugin) fills their offsets from the live schema once, when it loads:

```cpp
runtime.Entities.Pawn(slot).SetHealth(100);   // CBaseEntity::m_iHealth at the live offset
```

`schemagen` reads `gamedata/schema/manifest.json` plus a dump and writes
`include/VoltMod/Schema/Generated/` and `src/Schema/Generated/`. The dump comes from the
engine's network serializers, which exist only while a map runs, so the host writes
`addons/voltmod/gamedata/dumps/schema.json` at map start and skips the write when the file on disk already
carries the running game build.

The generated sources hold no offsets, so they are the same from a Windows or a Linux dump.
`--platform` only picks which committed baseline, `gamedata/schema/server.<platform>.json`, the
dump refreshes; `gamedata check --fix` reads it to name struct offsets. Re-rendering from a baseline
needs no server:

```bash
voltmod framework schemagen --dump gamedata/schema/server.windows.json --platform windows
```

Field access on a wrapper is what @ref sdk_players_guide "Entities and players" describes.

### What the load checks

`src/Schema/Generated/Layout.cpp` lists every field the accessors use, with the size the dump gave
it. At load each module looks every field up by name in the live schema, takes its offset, and
compares its size. A struct embedded by value has no size check, because its size differs per
platform and its own fields are listed. A moved field passes. A field that is gone or resized fails
the load, naming each one:

```text
SchemaLayout: the server schema no longer matches these fields; regenerate with voltmod framework schemagen:
  CCSPlayerPawn::m_ArmorValue: gone
  CBaseEntity::m_vecAbsVelocity: size 12 -> 16
```

The host logs the same check for its own copy as `Schema: every generated field found on game build <n>`.

## After a game update

`voltmod doctor --server <dir>` says when the server is behind Steam, and when gamedata was
checked on another build than the server runs. `voltmod serve` puts back the VoltMod line an update
removes from `gameinfo.gi`.

A moved schema field needs nothing. Only when the load names a gone or resized field: let a map load
so the host writes the dump, run `voltmod framework schemagen`, review the `git diff` of the
generated code, fix what used the field, rebuild.

Gamedata is repaired separately, and offline:

1. `voltmod framework gamedata fetch` downloads the new build's `server` and `engine2` binaries for both
   platforms into `~/.voltmod/cs2-builds/<build>/<platform>` (`CS2_BUILD_ARCHIVE` moves it). It
   also files the local server's `resolved.json` under the archived build it names, as
   `resolved.<platform>.json`, so run it before updating the server. Steam serves only the current
   build, so this archive is the only way to compare an update with the build before it.
2. `voltmod framework gamedata check --game-dir ~/.voltmod/cs2-builds/<build>/<platform>` reports which
   `functions` and `globals` patterns no longer match those binaries, and why. It needs no server.
3. `voltmod framework gamedata check --fix` repairs what it can, then read the diff.
4. Re-check every vtable index by hand. The slot check catches an index landing on data, not
   a valid slot holding the wrong function. `script` entries need no check: the load log says when
   a binding is gone.
5. Re-check every byte offset by hand. A stale offset reads plausible unrelated data.
6. Exercise each feature on a live server. Resolution is not correctness.
7. Update `build.server` and `build.verified` in the same change.

`check` takes `--game-dir` (default `CS2_SERVER_PATH`) and `--platform`. The platform otherwise
follows whichever binaries that directory holds.

What `--fix` will and will not do:

- An entry that still matches once is never rewritten. The file changes only where it was wrong.
- A missed entry is repaired only by widening one struct displacement - bytes a pattern should
  never have pinned - and only when exactly one such change brings it back to a single match. Two
  viable candidates means nothing can say which drifted, so it refuses.
- It never searches for a function. Re-sync those by hand from the upstream named in the file.

```text
==> gamedata windows (game build 2000908)
    21/22 patterns hold
    REPAIRED  functions.CCSCustomHudLayout::SetInputCaptureEnabled
              bytes 12-15: 1200 -> 1208, wildcarded
              1208 is CCSCustomHudLayout::m_vecPlayerLayoutStates
```

Common drift points:

| Entry | Section | Used by | Drift symptom |
| --- | --- | --- | --- |
| `CPlayer_MovementServices::RunCommand` | vtables | @ref VoltMod::Movement | Crash on the first movement tick, unless the slot check catches it |
| `CBaseEntity::Teleport` | vtables | @ref VoltMod::Teleport | Subscribing to `Teleported` is refused; `Teleport::Available` says why |
| `CServerSideClient` | vtables (table only) | @ref VoltMod::ClientConVars, addon downloads | Missing: `ClientConVars::Available` fails and addons are not sent |
| `CServerSideClient::INetworkMessageProcessingPreFilter` | vtables (table only, base) | @ref VoltMod::ScreenManager | Missing: presses never arrive |
| `CUserCmd::CSGOUserCmdPB` | offsets | @ref VoltMod::Movement | Missing: `Valid=false`. Stale: garbage viewangles and buttons |
| `CUserCmdBase::cmdNum` | offsets | `PlayerInput::CommandNumber` | Missing: falls back to `legacy_command_number`, which live clients leave at 0 |
| `CNetworkGameServer::ReplyConnection` | functions | @ref VoltMod::MultiAddonManager | `Add` logs the reason in the host and requires nothing |
| `CSource2Server::g_GameEventManager` | globals | @ref VoltMod::GameEvents | Events do not fire and center HTML does not display |

## Class table lookup

The host finds a class vtable by RTTI on Windows and by `_ZTV` symbol - falling back to the mapped
module's Itanium RTTI - on Linux. A `class` name must therefore be the top-level RTTI name exactly;
a nested or namespaced name resolves to nothing and the entry does not bind.

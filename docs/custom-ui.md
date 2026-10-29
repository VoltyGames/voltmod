# Custom UI layouts {#custom_ui_guide}

[TOC]

@ref VoltMod::ScreenManager (`runtime.Screens`) shows CS2 Panorama layouts through
`custom_hud_layout` entities. The layout itself - compiled XML and CSS - is authored and shipped
separately (@ref panorama_guide); this page is the C++ that drives one.

```cpp
// App.hpp: one screen per player, made the first time something draws for them.
VoltMod::PlayerScreens _welcome{runtime.Screens, "welcome"};

// When a player should see it:
VoltMod::Screen& screen = _welcome.For(slot);
if (!screen.EnsureSpawned(slot))
    return;                   // the engine would not spawn it

screen.SetText(slot, "name", "Welcome");
screen.SetHidden(slot, "welcome_card", false);  // show it
screen.ShowCursor(slot, true);                  // make it clickable
```

Every screen belongs to one player, and every write names that player's slot, or
@ref VoltMod::EveryoneSlot, which means the same.

`SetText` writes the variable a `text="{s:name}"` Label reads. Variables live on the layout's root
element, whose id is the layout's own name, so a text write names only the variable. `SetClass` and
`SetHidden` name an element by its id.

Each write returns @ref VoltMod::Status. Check one-shot writes; a redraw may ignore the result,
since the screen logs the first failure per slot and the next redraw retries. Unchanged values are
not re-sent, so redrawing everything is cheap.

Writes never spawn. @ref VoltMod::Screen::EnsureSpawned spawns the entity on first use, respawns it
after a map change removed it, and respawns once the player list changes when the entity was spawned
without per-player state for its owner, so call it before a redraw. `Remove()` drops the entity now and the next `EnsureSpawned`
makes a new one; destroying the move-only `Screen` removes it for good. Several layouts can exist
at once and are independent.

`Create` and @ref VoltMod::PlayerScreens take a bare name (`"welcome"`) or a resource name under
`panorama/layout/custom_game/` with its source `.xml` extension. Anything else is an
`Error::Invalid` rather than a layout that renders nothing:

```text
welcome                                     expanded to the line below
panorama/layout/custom_game/welcome.xml     correct
panorama/layout/custom_game/welcome.vxml_c  rejected: name the source, not the compiled resource
panorama/layout/hud/welcome.xml             rejected: outside the whitelisted directory
```

`m_strLayout` is a resource name, not markup, so a client renders only a layout it already has on
disk, and `gameinfo.gi`'s addon whitelist allows Panorama layouts nowhere but that directory.

## Reacting to a press

A button press arrives as @ref VoltMod::ButtonPress on @ref VoltMod::ScreenManager::Pressed.
Subscribing installs the hook, so keep what subscribing returns:

```cpp
_subs.Add(runtime.Screens.Pressed += [this](const VoltMod::ButtonPress& press) {
    if (press.ButtonId == "welcome_accept")
        Accept(press.Slot);
});
```

`Pressed` carries presses from every layout, so compare `ButtonId` against ids you authored;
starting every id with the layout name keeps two layouts apart, which is also what
@ref panorama_guide enforces. `ButtonId` is client-controlled text - never parse anything out of
it. `Slot` is the client that pressed, whatever pawn it is watching.

A press is raised on the game frame after it arrives, not inside the engine's inbound message
processing, so a handler may write to the screen - hide it, drop the cursor - and the write reaches
the client.

Nothing is clickable until that player has a cursor: `ShowCursor(slot, true)`. Without it the game
keeps mouse-look and the layout never sees a pointer, which is the usual reason a layout renders
but does nothing.

## One screen per player

A screen stays with its player whatever they are watching, so a menu survives death and spectating.
There is no screen everyone receives: it would send each player's state to every client.

```cpp
auto screen = runtime.Screens.Create("admin_menu", slot);
if (screen && screen->EnsureSpawned(slot))
    screen->SetText(slot, "title", "Only you see this");
```

The entity is networked to its owner alone through the Visibility filter. Its text and class writes
land in the layout's global state, which a client shows whatever pawn it is watching; the cursor
goes to the owner's own per-player state. A write naming any other slot is ignored. The screen
removes its entity when the slot changes hands.

It costs one entity per player, so create one when something first draws for a player rather than
one per connected player. @ref VoltMod::PlayerScreens does that for one layout:

```cpp
VoltMod::PlayerScreens _menus{runtime.Screens, "admin_menu"};

VoltMod::Screen& screen = _menus.For(slot);   // created on first use
if (screen.EnsureSpawned(slot))
    screen.SetHidden(slot, "admin_menu", false);
```

A screen `Create` refuses is logged once and stays empty, so its writes fail quietly. `Find(slot)`
returns what was already created, or null, and never creates. `FindSpawned(slot)` creates and spawns
it, and returns null for a bot or an empty slot: a screen never spawns without a client to draw on.

```cpp
if (VoltMod::Screen* screen = _hud.FindSpawned(slot))
    screen->SetText(slot, "balance", "$800");
```

A screen that takes clicks, such as a shop, opens and closes through the same object. `Open(slot)`
spawns it, shows the root element and gives the player a cursor; `Close(slot)` takes the cursor back
and removes the entity, since a client draws only its first `custom_hud_layout` and a hidden one
would block the next. `IsOpen(slot)` says whether the player is using it, for code that reads their
keys and should leave them alone meanwhile:

```cpp
if (VoltMod::Screen* screen = _shop.Open(slot))
    Draw(*screen, slot);
// ...
if (_shop.IsOpen(slot))
    return;  // the key belongs to the screen
```

`IsOpen` turns false on `Close`, when the slot changes hands, and when a map change removes the
entity. `runtime.Screens.HasCursor(slot)` asks the same of every screen the player has, for key
handling that should not know which screens exist.

Put the writes a screen needs behind a class of your own - `SetRow(slot, index, row)` reads better
at a call site than raw element ids, and the generated header (@ref panorama_guide) supplies every
id it uses. A clickable menu on a player screen is @ref VoltMod::PanoramaMenu drawing through such a
class; see @ref menus_guide.

## Naming elements and classes

Reuse element ids, class names and variable names. Each distinct name is permanently interned in a
1024-entry client-side table, and generating names during a redraw eventually stops updates.
`voltmod panorama check` counts them for you.

## Availability

`runtime.Screens.Available()` says whether screens can be drawn and pressed, and names the first
gamedata entry that did not bind:

| Entry | Missing means |
| --- | --- |
| the three `CCSCustomHudLayout` setters | spawning still works, writes fail |
| `CServerSideClient::INetworkMessageProcessingPreFilter` | presses never arrive |

All are located in `server.dll` / `engine2`, on Windows and on Linux, and stop binding when a
pattern stops matching after a game update - re-verify `gamedata/gamedata.jsonc` before looking
anywhere else. Schema fields resolve themselves by name, so an offset that moves costs nothing
here; the fields declare their expected size instead, and a mismatch warns once at resolve time.

## Why writes are calls, not netvar pokes

Each of the entity's three networked string tables is shadowed by a server-only `CUtlHashtable`
that is neither in the schema nor networked. Appending to a vector by hand leaves those indexes
stale, so the next engine-side call misses the hash, appends a duplicate, and the state silently
desyncs. Every write therefore goes through the game's own setter, which interns, dedupes and
notifies correctly.

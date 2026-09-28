"""A plugin's workshop content: compile a folder of it, and export what the server needs."""

import re
import shutil
from pathlib import Path

from voltmod import console
from voltmod.errors import VoltmodError
from voltmod.workshop_tools import AddonDirs, compile_resources

# Compiled from their own source file; materials, textures and sounds come along with these.
COMPILED_SOURCES = (".vmdl", ".vpcf", ".vsndevts", ".vdata")
# Kept beside a model but never named by a compiled file.
NEVER_REFERENCED = (".blend", *COMPILED_SOURCES)
REFERENCE = re.compile(rb"[\w/.-]+\.(?:dmx|vmat|png|tga|psd|jpg)", re.IGNORECASE)
# What the server itself loads: collision, hitboxes and attachments; effects and sound events
# spawned by name; entity subclasses. Textures and sounds only render or play on clients.
SERVER_SUFFIXES = (".vmdl_c", ".vpcf_c", ".vsndevts_c", ".vdata_c")


def content_dir(root: Path, plugin: str) -> Path:
    return root / "plugins" / plugin / "content"


def server_assets_dir(root: Path, plugin: str) -> Path:
    return root / "plugins" / plugin / "server-assets"


def compile_folder(source: Path, folder: Path, dirs: AddonDirs, prune: bool) -> Path:
    """Compile one folder of `source` in the Workshop Tools; returns its compiled folder."""
    files = source / folder
    if not files.is_dir():
        raise VoltmodError(f"no folder at {files}")

    for path in _unreferenced(files):
        if prune:
            path.unlink()
            console.note(f"pruned {path.name}")
        else:
            console.note(f"unreferenced {path.name} (--prune deletes it)")

    staged = dirs.sources / folder
    mirror(files, staged, skip=(".blend",))
    # Wiped, so a renamed texture leaves nothing behind.
    compiled = dirs.compiled / folder
    shutil.rmtree(compiled, ignore_errors=True)
    compile_resources(dirs, sorted(p for p in staged.iterdir() if p.suffix in COMPILED_SOURCES))
    return compiled


def export_server_assets(root: Path, plugin: str, dirs: AddonDirs) -> int:
    """Replace the plugin's server-assets/ with the compiled files the server loads.

    A compiled file whose source left the plugin's content/ is left out, so a stale compile
    never ships.
    """
    if not dirs.compiled.is_dir():
        raise VoltmodError(f"no compiled addon at {dirs.compiled}; compile it first")
    source = content_dir(root, plugin)
    destination = server_assets_dir(root, plugin)
    shutil.rmtree(destination, ignore_errors=True)
    count = 0
    for file in sorted(dirs.compiled.rglob("*")):
        relative = file.relative_to(dirs.compiled)
        # Tool caches such as _bakeresourcecache hold compiled copies too.
        wanted = file.is_file() and file.suffix in SERVER_SUFFIXES
        if not wanted or relative.parts[0].startswith("_") or not has_source(source, relative):
            continue
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(file, target)
        count += 1
    return count


def has_source(source: Path, compiled: Path) -> bool:
    """Whether `compiled` (relative, such as models/x/x.vmdl_c) still has its source file."""
    return (source / compiled.with_suffix(compiled.suffix.removesuffix("_c"))).is_file()


def mirror(source: Path, target: Path, skip: tuple[str, ...] = ()) -> None:
    """Make `target` hold exactly the files of `source`, minus the suffixes in `skip`."""
    if target.exists() and target.resolve() == source.resolve():
        return
    target.mkdir(parents=True, exist_ok=True)
    wanted = {p.name for p in source.iterdir() if p.is_file() and p.suffix.lower() not in skip}
    for stale in target.iterdir():
        if stale.is_file() and stale.name not in wanted:
            stale.unlink()
    for name in wanted:
        shutil.copy2(source / name, target / name)


def _unreferenced(folder: Path) -> list[Path]:
    """Files in `folder` that no .vmdl, .vmat, .vpcf or DMX beside them names."""
    names: set[str] = set()
    for path in folder.iterdir():
        if path.suffix.lower() in (".vmdl", ".vmat", ".dmx", ".vpcf"):
            for match in REFERENCE.findall(path.read_bytes()):
                names.add(Path(match.decode(errors="ignore")).name.lower())
    return sorted(
        path
        for path in folder.iterdir()
        if path.is_file()
        and path.suffix.lower() not in NEVER_REFERENCED
        and path.name.lower() not in names
    )

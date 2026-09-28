from pathlib import Path

from voltmod.content.content import export_server_assets, mirror
from voltmod.project import Plugin
from voltmod.workshop_tools import AddonDirs


def write(path: Path, text: str = "x") -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def test_server_assets_keep_only_server_files_with_a_source(tmp_path):
    content = tmp_path / "plugins/demo/content"
    write(content / "models/demo/crate.vmdl")
    write(content / "scripts/weapons.vdata")

    dirs = AddonDirs.of(tmp_path / "client", "demo")
    write(dirs.compiled / "models/demo/crate.vmdl_c")
    write(dirs.compiled / "models/demo/crate_color_png_1234.vtex_c")  # client only
    write(dirs.compiled / "particles/demo/gone.vpcf_c")  # its source left content/
    write(dirs.compiled / "scripts/weapons.vdata_c")
    write(dirs.compiled / "_bakeresourcecache/models/demo/crate.vmdl_c")  # tool cache

    assert export_server_assets(Plugin("demo", tmp_path / "plugins/demo"), dirs) == 2
    exported = tmp_path / "plugins/demo/server-assets"
    assert sorted(
        p.relative_to(exported).as_posix() for p in exported.rglob("*") if p.is_file()
    ) == [
        "models/demo/crate.vmdl_c",
        "scripts/weapons.vdata_c",
    ]


def test_mirror_drops_files_the_source_no_longer_has(tmp_path):
    source = tmp_path / "source"
    target = tmp_path / "target"
    write(source / "crate.vmdl")
    write(source / "crate.blend")
    write(target / "old.vmat")

    mirror(source, target, skip=(".blend",))

    assert sorted(p.name for p in target.iterdir()) == ["crate.vmdl"]

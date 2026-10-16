import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from voltmod.bundled import load_template
from voltmod.errors import VoltmodError
from voltmod.files import write_or_check
from voltmod.framework.paths import (
    GENERATED_HEADER_DIR,
    GENERATED_SOURCE_DIR,
    SCHEMA_BASELINES,
    SCHEMA_HEADER_DIR,
)
from voltmod.framework.schemagen.accessors import AccessorCode, accessor_code
from voltmod.framework.schemagen.dump_types import Dump, DumpedEnum, Manifest
from voltmod.framework.schemagen.fields import TYPE_INCLUDES
from voltmod.framework.schemagen.model import (
    ENTITY_BASE,
    OWNER_LINK_FIELD,
    FieldKind,
    SchemaClass,
    SchemaField,
    cpp_identifier,
    enum_underlying_type,
    offset_constant,
)
from voltmod.framework.schemagen.resolve import (
    base_classes,
    collect_enums,
    resolve_classes,
    trimmed_dump,
)
from voltmod.toolchain.clang_format import format_cpp_texts


@dataclass(frozen=True, slots=True)
class LayoutRow:
    """One field whose offset the live schema fills at load; size 0 skips the size check."""

    class_name: str
    field_name: str
    size: int


@dataclass(frozen=True, slots=True)
class GeneratedClass:
    schema_class: SchemaClass
    fields: list[tuple[SchemaField, AccessorCode | None]]  # None where the field is skipped

    @property
    def accessors(self) -> list[AccessorCode]:
        return [code for _, code in self.fields if code]


@dataclass(frozen=True, slots=True)
class SchemaFiles:
    files: dict[Path, str]  # repo-relative path -> text; C++ is formatted when written
    summary: str


def render_schema(dump: Dump, manifest: Manifest, platform: str) -> SchemaFiles:
    classes = resolve_classes(dump, manifest)
    enums = collect_enums(dump, classes)
    ordered = [classes[name] for name in sorted(classes)]
    game_build = dump.get("build", "unknown")

    generated = {
        schema_class.name: GeneratedClass(
            schema_class,
            [(field, accessor_code(schema_class, field)) for field in schema_class.fields],
        )
        for schema_class in ordered
    }
    rows = layout_rows(dump, ordered)
    row_indices = {(row.class_name, row.field_name): index for index, row in enumerate(rows)}
    files: dict[Path, str] = {}
    for schema_class in ordered:
        shared = {
            "schema_class": schema_class,
            "fields": generated[schema_class.name].fields,
            "entity_base": ENTITY_BASE,
            "offset_constant": offset_constant,
            "owner_link_field": OWNER_LINK_FIELD,
            "row_index": lambda class_name, field_name: row_indices[(class_name, field_name)],
        }
        files[GENERATED_HEADER_DIR / f"{schema_class.name}.hpp"] = _render(
            "class.hpp.j2", includes=_header_includes(schema_class), **shared
        )
        files[GENERATED_SOURCE_DIR / f"{schema_class.name}.cpp"] = _render(
            "class.cpp.j2", includes=_source_includes(schema_class), **shared
        )
    files[GENERATED_HEADER_DIR / "Enums.hpp"] = _render("enums.hpp.j2", enums=_enum_listings(enums))
    files[SCHEMA_HEADER_DIR / "Api.hpp"] = _render("api.hpp.j2", classes=ordered)
    files[GENERATED_SOURCE_DIR / "Layout.cpp"] = _render("layout.cpp.j2", rows=rows)
    for wrapper, names in manifest.get("wrappers", {}).items():
        wrapped = _wrapped_classes(wrapper, names, generated)
        files[GENERATED_HEADER_DIR / "Wrappers" / f"{wrapper}.inc"] = _render(
            "wrapper.inc.j2", wrapper=wrapper, classes=wrapped
        )
    baseline = trimmed_dump(dump, classes, enums)
    files[SCHEMA_BASELINES[platform]] = json.dumps(baseline, indent=2) + "\n"
    return SchemaFiles(files, _summary(classes, enums, game_build))


def layout_rows(dump: Dump, classes: list[SchemaClass]) -> list[LayoutRow]:
    """Every generated field, plus the owner link on each class that declares one."""
    rows = []
    for schema_class in sorted(classes, key=lambda entry: entry.name):
        rows += [
            # An embedded struct's size differs per platform; its own fields are rows.
            LayoutRow(schema_class.name, field.engine_name, 0 if field.embedded else field.size)
            for field in schema_class.generated_fields
        ]
        if schema_class.owner_link_offset >= 0:
            size = _owner_link_size(dump, schema_class.name)
            rows.append(LayoutRow(schema_class.name, OWNER_LINK_FIELD, size))
    return rows


def _owner_link_size(dump: Dump, name: str) -> int:
    """The owner link's size from the class or base declaring it, as the live lookup finds it."""
    for owner in [name, *base_classes(dump, name)]:
        for dumped in dump["classes"][owner]["fields"]:
            if dumped["name"] == OWNER_LINK_FIELD:
                return dumped["size"]
    return 0


def write_schema(repo: Path, files: dict[Path, str], *, check: bool = False) -> None:
    """Format and write `files`, deleting stale ones; with `check`, fail on any difference."""
    expected = {repo / relative for relative in files}
    stale = [path for path in _existing_generated_files(repo) if path not in expected]
    if check and stale:
        raise VoltmodError(f"stale generated files: {', '.join(map(str, stale))}")
    for path in stale:
        path.unlink()

    for relative, text in format_cpp_texts(repo, files).items():
        write_or_check(repo / relative, text, check=check)


def _render(template: str, **fields: Any) -> str:
    return load_template(f"schemagen/{template}").render(**fields)


def _existing_generated_files(repo: Path) -> list[Path]:
    headers = [path for path in (repo / GENERATED_HEADER_DIR).rglob("*") if path.is_file()]
    return headers + sorted((repo / GENERATED_SOURCE_DIR).rglob("*.cpp"))


def _header_includes(schema_class: SchemaClass) -> list[str]:
    includes = {"<VoltMod/Schema/View.hpp>", "<cstdint>"}
    if schema_class.base:
        includes.add(f"<VoltMod/Schema/Generated/{schema_class.base}.hpp>")
    for field in schema_class.fields:
        match field.kind:
            case FieldKind.VIEW:
                includes.add(f"<VoltMod/Schema/Generated/{field.view_class}.hpp>")
            case FieldKind.ENUM:
                includes.add("<VoltMod/Schema/Generated/Enums.hpp>")
            case FieldKind.CHAR_ARRAY:
                includes.add("<string_view>")
            case FieldKind.ARRAY:
                includes.add("<cstddef>")
                if field.cpp_type in TYPE_INCLUDES:
                    includes.add(TYPE_INCLUDES[field.cpp_type])
            case FieldKind.VALUE | FieldKind.HANDLE if field.cpp_type in TYPE_INCLUDES:
                includes.add(TYPE_INCLUDES[field.cpp_type])
    return sorted(includes)


def _source_includes(schema_class: SchemaClass) -> list[str]:
    includes = {
        "<VoltMod/Engine/Memory/MemoryAccess.hpp>",
        f"<VoltMod/Schema/Generated/{schema_class.name}.hpp>",
        '"Schema/Layout.hpp"',
        '"Schema/Notify.hpp"',
    }
    for field in schema_class.fields:
        if field.kind is FieldKind.VIEW:
            includes.add(f"<VoltMod/Schema/Generated/{field.view_class}.hpp>")
        if field.kind is FieldKind.CHAR_ARRAY:
            includes.add("<VoltMod/Core/Text/CharBuf.hpp>")
    return sorted(includes)


def _enum_listings(enums: dict[str, DumpedEnum]) -> list[dict[str, Any]]:
    listings = []
    for name, info in enums.items():
        seen: set[int] = set()
        items = []
        for item in info["items"]:
            # C++ rejects duplicate enumerators; the schema allows aliases.
            alias = item["value"] in seen
            items.append({"name": item["name"], "value": item["value"], "alias": alias})
            seen.add(item["value"])
        listings.append(
            {
                "schema_name": name,
                "name": cpp_identifier(name),
                "underlying_type": enum_underlying_type(info["size"]),
                "items": items,
            }
        )
    return listings


def _wrapped_classes(
    wrapper: str, names: list[str], generated: dict[str, GeneratedClass]
) -> list[GeneratedClass]:
    """The named classes that generate at least one accessor, in the manifest's order."""
    for name in names:
        if name not in generated:
            raise VoltmodError(f"wrapper '{wrapper}' names '{name}', which is not generated")
    return [generated[name] for name in names if generated[name].accessors]


def _summary(classes: dict[str, SchemaClass], enums: dict[str, DumpedEnum], game_build: str) -> str:
    generated = sum(len(schema_class.generated_fields) for schema_class in classes.values())
    skipped = sum(len(schema_class.fields) for schema_class in classes.values()) - generated
    line = (
        f"schemagen: game build {game_build}, {len(classes)} classes, "
        f"{len(enums)} enums, {generated} fields"
    )
    return line + (f", {skipped} skipped" if skipped else "")

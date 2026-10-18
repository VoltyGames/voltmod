import json
from pathlib import Path

import pytest

from voltmod.errors import VoltmodError
from voltmod.framework.paths import (
    GENERATED_HEADER_DIR,
    GENERATED_SOURCE_DIR,
    SCHEMA_BASELINES,
    SCHEMA_MANIFEST,
)
from voltmod.framework.schemagen.generate import (
    layout_rows,
    render_schema,
    write_schema,
)
from voltmod.framework.schemagen.model import method_name
from voltmod.framework.schemagen.resolve import collect_enums, resolve_classes, trimmed_dump

REPO_ROOT = Path(__file__).resolve().parents[2]


def type_info(name, category="SCHEMA_TYPE_BUILTIN", **extra):
    return {"name": name, "category": category, **extra}


def dumped_field(name, offset, size, info, networked=True):
    return {"name": name, "offset": offset, "size": size, "type": info, "networked": networked}


def dump():
    """An entity, a chained component, an embedded struct and the types they reach."""
    # fmt: off
    return {
        "classes": {
            "CEntityInstance": {"size": 48, "base": "", "chain_offset": -1, "fields": []},
            "CBaseEntity": {
                "size": 1192,
                "base": "CEntityInstance",
                "chain_offset": -1,
                "fields": [
                    dumped_field("m_iHealth", 720, 4, type_info("int32")),
                    dumped_field("m_lifeState", 728, 1, type_info("uint8")),
                    dumped_field(
                        "m_MoveType", 755, 1,
                        type_info("MoveType_t", "SCHEMA_TYPE_DECLARED_ENUM"),
                    ),
                    dumped_field(
                        "m_hGroundEntity", 1004, 4,
                        type_info(
                            "CHandle< CBaseEntity >",
                            "SCHEMA_TYPE_ATOMIC",
                            atomic="SCHEMA_ATOMIC_T",
                            inner="CBaseEntity",
                        ),
                    ),
                    dumped_field(
                        "m_pServices", 40, 8,
                        type_info("CMoneyServices*", "SCHEMA_TYPE_POINTER", inner="CMoneyServices"),
                    ),
                    dumped_field(
                        "m_state", 300, 8,
                        type_info("CEmbedded", "SCHEMA_TYPE_DECLARED_CLASS"),
                    ),
                    dumped_field(
                        "m_vecStuff", 400, 24,
                        type_info(
                            "CUtlVector< int >",
                            "SCHEMA_TYPE_ATOMIC",
                            atomic="SCHEMA_ATOMIC_COLLECTION_OF_T",
                            inner="int32",
                        ),
                    ),
                    dumped_field(
                        "m_szName", 500, 32,
                        type_info("char[32]", "SCHEMA_TYPE_FIXED_ARRAY", inner="char", extent=32),
                    ),
                    dumped_field(
                        "m_nSlots", 600, 20,
                        type_info("int32[5]", "SCHEMA_TYPE_FIXED_ARRAY", inner="int32", extent=5),
                    ),
                    dumped_field("m_bits", 700, 4, type_info("bitfield:3", "SCHEMA_TYPE_BITFIELD")),
                ],
            },
            "CMoneyServices": {
                "size": 88,
                "base": "",
                "chain_offset": 8,
                "fields": [
                    dumped_field(
                        "__m_pChainEntity", 8, 40,
                        type_info("CNetworkVarChainer", "SCHEMA_TYPE_DECLARED_CLASS"),
                    ),
                    dumped_field("m_iAccount", 72, 4, type_info("int32")),
                ],
            },
            "CEmbedded": {
                "size": 8,
                "base": "",
                "chain_offset": -1,
                "fields": [dumped_field("m_bFlag", 4, 1, type_info("bool"))],
            },
            "CLonely": {
                "size": 16,
                "base": "",
                "chain_offset": -1,
                "fields": [dumped_field("m_iValue", 0, 4, type_info("int32"))],
            },
        },
        "enums": {
            "MoveType_t": {
                "size": 1,
                "items": [
                    {"name": "MOVETYPE_NONE", "value": 0},
                    {"name": "MOVETYPE_WALK", "value": 2},
                ],
            },
            "Unused_t": {"size": 4, "items": [{"name": "UNUSED", "value": 0}]},
        },
    }
    # fmt: on


def manifest(classes=None):
    return {
        "classes": classes
        if classes is not None
        else {
            "CBaseEntity": [
                "m_iHealth>Health",
                "m_lifeState",
                "m_MoveType",
                "m_hGroundEntity",
                "m_pServices>Services",
                "m_state>State",
                "m_vecStuff>Stuff",
                "m_szName>Name",
                "m_nSlots>Slots",
                "m_bits",
            ],
            "CMoneyServices": ["m_iAccount>Account"],
            "CEmbedded": ["m_bFlag>Flag"],
        },
        "wrappers": {"Entity": ["CBaseEntity"]},
    }


def class_header(dumped, selected, name):
    return render_schema(dumped, selected, "windows").files[GENERATED_HEADER_DIR / f"{name}.hpp"]


@pytest.mark.parametrize("platform", list(SCHEMA_BASELINES))
def test_the_committed_generated_tree_is_what_the_generator_writes(platform):
    """Either platform's baseline must regenerate every committed file byte for byte."""
    baseline = json.loads((REPO_ROOT / SCHEMA_BASELINES[platform]).read_text(encoding="utf-8"))
    shipped = json.loads((REPO_ROOT / SCHEMA_MANIFEST).read_text(encoding="utf-8"))
    files = render_schema(baseline, shipped, platform).files
    write_schema(REPO_ROOT, files, check=True)


def sources(dumped, selected=None):
    """The generated C++, without the baseline the dump refreshes."""
    files = render_schema(dumped, selected or manifest(), "windows").files
    return {path: text for path, text in files.items() if path.suffix != ".json"}


def test_both_platforms_generate_the_same_sources():
    shipped = json.loads((REPO_ROOT / SCHEMA_MANIFEST).read_text(encoding="utf-8"))
    windows, linux = (
        json.loads((REPO_ROOT / SCHEMA_BASELINES[platform]).read_text(encoding="utf-8"))
        for platform in ("windows", "linux")
    )
    assert sources(windows, shipped) == sources(linux, shipped)


def moved_field():
    dumped = dump()
    dumped["classes"]["CBaseEntity"]["fields"][0]["offset"] += 8
    return dumped


def moved_owner_link():
    dumped = dump()
    dumped["classes"]["CMoneyServices"]["fields"][0]["offset"] += 8
    return dumped


def grown_class():
    dumped = dump()
    dumped["classes"]["CBaseEntity"]["size"] += 64
    return dumped


@pytest.mark.parametrize("mutate", [moved_field, moved_owner_link, grown_class])
def test_a_moved_field_changes_no_generated_source(mutate):
    """Offsets come from the live schema at load; only a new, gone or resized field regenerates."""
    assert sources(mutate()) == sources(dump())


def test_layout_rows_check_every_size_but_an_embedded_struct():
    dumped = dump()
    rows = {
        (row.class_name, row.field_name): row.size
        for row in layout_rows(dumped, list(resolve_classes(dumped, manifest()).values()))
    }
    assert rows[("CBaseEntity", "m_iHealth")] == 4
    assert rows[("CBaseEntity", "m_pServices")] == 8
    assert rows[("CBaseEntity", "m_state")] == 0, "its size differs per platform"
    assert rows[("CMoneyServices", "__m_pChainEntity")] == 40
    assert ("CBaseEntity", "m_bits") not in rows, "a skipped field generates nothing"


def test_each_offset_reads_its_own_layout_row():
    dumped = dump()
    rows = layout_rows(dumped, list(resolve_classes(dumped, manifest()).values()))
    index = {(row.class_name, row.field_name): position for position, row in enumerate(rows)}
    files = sources(dumped)
    entity = files[GENERATED_SOURCE_DIR / "CBaseEntity.cpp"]
    money = files[GENERATED_SOURCE_DIR / "CMoneyServices.cpp"]
    health = index[("CBaseEntity", "m_iHealth")]
    owner = index[("CMoneyServices", "__m_pChainEntity")]
    assert f"kCBaseEntity_Health = GeneratedFields[{health}].Offset;" in entity
    assert f"CMoneyServices_kOwnerLinkOffset = GeneratedFields[{owner}].Offset;" in money
    assert '.Field = "m_iHealth", .Size = 4}' in files[GENERATED_SOURCE_DIR / "Layout.cpp"]


def test_the_closure_pulls_in_bases_and_returned_types_but_nothing_else():
    classes = resolve_classes(dump(), manifest())
    assert "CEntityInstance" in classes, "a base must be generated so the C++ chain matches"
    assert "CMoneyServices" in classes and "CEmbedded" in classes
    assert "CLonely" not in classes


def test_only_what_the_generator_read_reaches_the_baseline():
    dumped = dump()
    classes = resolve_classes(dumped, manifest())
    enums = collect_enums(dumped, classes)

    baseline = trimmed_dump(dumped, classes, enums)
    assert set(baseline["classes"]) == set(classes)
    assert set(baseline["enums"]) == {"MoveType_t"}


def test_an_unmapped_type_is_skipped_visibly():
    header = class_header(dump(), manifest(), "CBaseEntity")
    assert "// skipped: m_bits (bitfield:3)" in header


def test_a_struct_embedded_in_an_entity_keeps_its_setters():
    assert resolve_classes(dump(), manifest())["CEmbedded"].in_entity is True
    assert "void SetFlag(bool value) const" in class_header(dump(), manifest(), "CEmbedded")


def test_a_struct_embedded_in_an_embedded_struct_keeps_its_setters():
    dumped = dump()
    dumped["classes"]["CEmbedded"]["fields"].append(
        dumped_field("m_inner", 6, 2, type_info("CInner", "SCHEMA_TYPE_DECLARED_CLASS"))
    )
    dumped["classes"]["CInner"] = {
        "size": 2,
        "base": "",
        "chain_offset": -1,
        "fields": [dumped_field("m_nMask", 1, 1, type_info("uint8"))],
    }
    selected = manifest()
    selected["classes"]["CEmbedded"].append("m_inner>Inner")
    selected["classes"]["CInner"] = ["m_nMask>Mask"]
    assert resolve_classes(dumped, selected)["CInner"].in_entity is True
    assert "void SetMask(uint8_t value) const" in class_header(dumped, selected, "CInner")


def test_a_field_the_engine_does_not_network_is_written_without_a_notify():
    dumped = dump()
    dumped["classes"]["CBaseEntity"]["fields"][1]["networked"] = False
    source = sources(dumped)[GENERATED_SOURCE_DIR / "CBaseEntity.cpp"]
    assert "*MemberPtr<uint8_t>(_base, kCBaseEntity_LifeState) = value;" in source
    assert "_ownerOffset + kCBaseEntity_LifeState" not in source
    assert "_ownerOffset + kCBaseEntity_Health" in source


def test_a_class_with_no_route_to_replicate_writes_is_read_only():
    dumped = dump()
    dumped["classes"]["CBaseEntity"]["base"] = ""
    header = class_header(dumped, manifest(), "CEmbedded")
    assert "bool Flag() const" in header
    assert "SetFlag" not in header


def test_a_fixed_array_of_handles_reads_and_writes_entity_refs():
    dumped = dump()
    handle = "CHandle< CBaseEntity >"
    handles = type_info(f"{handle}[4]", "SCHEMA_TYPE_FIXED_ARRAY", inner=handle, extent=4)
    dumped["classes"]["CBaseEntity"]["fields"].append(dumped_field("m_hTargets", 800, 16, handles))
    selected = manifest({"CBaseEntity": ["m_hTargets>TargetRef"]})
    header = class_header(dumped, selected, "CBaseEntity")
    assert "EntityRef TargetRef(size_t index) const;" in header
    assert "void SetTargetRef(size_t index, EntityRef value) const;" in header
    assert "#include <VoltMod/Engine/EntityRef.hpp>" in header


def test_a_type_override_reads_the_leading_value_of_a_larger_field():
    selected = manifest({"CBaseEntity": ["m_state>Offset:Vector"]})
    header = class_header(dump(), selected, "CBaseEntity")
    assert "Vector Offset() const" in header


def test_a_star_takes_every_field_the_dump_reports():
    classes = resolve_classes(dump(), manifest({"CMoneyServices": "*"}))
    generated = classes["CMoneyServices"].generated_fields
    assert [field.engine_name for field in generated] == ["m_iAccount"]


@pytest.mark.parametrize(
    ("engine_name", "expected"),
    [
        ("m_flVelocityModifier", "VelocityModifier"),
        ("m_iAccount", "Account"),
        ("m_bOnGroundLastTick", "OnGroundLastTick"),
        ("m_angEyeAngles", "EyeAngles"),
        ("m_ArmorValue", "ArmorValue"),
        ("m_lifeState", "LifeState"),
        ("m_modelState", "ModelState"),
    ],
)
def test_the_method_name_strips_only_a_real_hungarian_prefix(engine_name, expected):
    assert method_name(engine_name) == expected


def test_two_fields_mapping_to_one_accessor_are_refused():
    with pytest.raises(VoltmodError, match="both map to"):
        resolve_classes(dump(), manifest({"CBaseEntity": ["m_iHealth>Same", "m_lifeState>Same"]}))


def test_a_manifest_field_the_dump_lacks_is_refused():
    with pytest.raises(VoltmodError, match="m_iNope"):
        resolve_classes(dump(), manifest({"CBaseEntity": ["m_iNope"]}))

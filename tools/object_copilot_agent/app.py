from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from fastapi import FastAPI
from pydantic import BaseModel, Field


app = FastAPI(title="Moon Object Copilot Agent", version="0.3.0")

REPO_ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = REPO_ROOT / "assets" / "objects"
INDEX_PATH = ASSET_ROOT / "index.json"


class AgentRequest(BaseModel):
    current_object_json: str = ""
    conversation: list[dict] = Field(default_factory=list)
    user_prompt: str = ""


class AgentResponse(BaseModel):
    summary: str
    updated_object_json: str
    notes: list[str] = Field(default_factory=list)


INDEX_JSON = json.loads(INDEX_PATH.read_text(encoding="utf-8"))
INDEX_ITEMS = INDEX_JSON.get("items", [])
INDEX_BY_ID = {
    item["id"]: item
    for item in INDEX_ITEMS
    if isinstance(item, dict) and isinstance(item.get("id"), str)
}
ALLOWED_MATERIALS = set(INDEX_JSON.get("materials", {}).get("allowed", []))

ZH = {
    "create": ["\u751f\u6210", "\u521b\u5efa", "\u505a\u4e00\u4e2a", "\u6765\u4e00\u4e2a", "\u65b0\u5efa"],
    "cylinder": ["\u5706\u67f1", "\u5706\u67f1\u4f53"],
    "sphere": ["\u7403", "\u7403\u4f53"],
    "cube": ["\u65b9\u5757", "\u7acb\u65b9\u4f53", "\u76d2\u5b50"],
    "capsule": ["\u80f6\u56ca"],
    "table": ["\u684c\u5b50", "\u4e66\u684c", "\u9910\u684c"],
    "chair": ["\u6905\u5b50"],
    "lamp": ["\u53f0\u706f", "\u706f"],
    "wheel": ["\u8f6e\u5b50", "\u8f6e\u80ce"],
    "bed": ["\u5e8a"],
    "sofa": ["\u6c99\u53d1"],
    "bookshelf": ["\u4e66\u67b6"],
    "cabinet": ["\u67dc\u5b50"],
    "coffee_table": ["\u8336\u51e0"],
    "fridge": ["\u51b0\u7bb1"],
    "television": ["\u7535\u89c6"],
    "bigger": ["\u5927\u4e00\u70b9", "\u5927\u4e00\u4e9b", "\u66f4\u5927"],
    "smaller": ["\u5c0f\u4e00\u70b9", "\u66f4\u5c0f"],
    "higher": ["\u9ad8\u4e00\u70b9", "\u66f4\u9ad8"],
    "wider": ["\u5bbd\u4e00\u70b9", "\u66f4\u5bbd"],
    "longer": ["\u957f\u4e00\u70b9", "\u66f4\u957f"],
    "thicker": ["\u539a\u4e00\u70b9", "\u66f4\u539a"],
    "leg": ["\u684c\u817f", "\u817f"],
    "tabletop": ["\u684c\u9762", "\u53f0\u9762"],
    "backrest": ["\u9760\u80cc"],
    "pole": ["\u6746", "\u706f\u6746", "\u8f66\u628a", "\u628a\u624b"],
    "shade": ["\u706f\u7f69"],
    "metal": ["\u91d1\u5c5e", "\u94a2", "\u94ec"],
    "wood": ["\u6728\u5934", "\u6728\u8d28", "\u6728\u5236"],
    "glass": ["\u73bb\u7483"],
    "plastic": ["\u5851\u6599"],
    "rubber": ["\u6a61\u80f6"],
}

PRESET_KEYWORDS = {
    "table_v1": ["table", "desk", *ZH["table"]],
    "simple_table_v1": ["simple table", "small table", "\u5c0f\u684c\u5b50", "\u7b80\u6613\u684c"],
    "chair_v1": ["chair", *ZH["chair"]],
    "desk_lamp_v1": ["lamp", "desk lamp", "light", *ZH["lamp"]],
    "wheel_v1": ["wheel", "tire", "tyre", *ZH["wheel"]],
    "bed_v1": ["bed", *ZH["bed"]],
    "sofa_v1": ["sofa", *ZH["sofa"]],
    "bookshelf_v1": ["bookshelf", "shelf", *ZH["bookshelf"]],
    "cabinet_v1": ["cabinet", *ZH["cabinet"]],
    "coffee_table_v1": ["coffee table", *ZH["coffee_table"]],
    "fridge_v1": ["fridge", "refrigerator", *ZH["fridge"]],
    "television_v1": ["tv", "television", *ZH["television"]],
}


def _contains_any(text: str, keywords: list[str]) -> bool:
    return any(keyword.lower() in text.lower() for keyword in keywords)


def _parse_blueprint(document: str) -> dict[str, Any]:
    parsed = json.loads(document)
    if not isinstance(parsed, dict):
        raise ValueError("Current object JSON must be a JSON object")
    return parsed


def _load_preset_blueprint(preset_id: str) -> dict[str, Any] | None:
    item = INDEX_BY_ID.get(preset_id)
    if not item:
        return None

    preset_path = ASSET_ROOT / item["path"]
    if not preset_path.exists():
        return None

    return json.loads(preset_path.read_text(encoding="utf-8"))


def _param_spec(value: float, minimum: float, maximum: float) -> dict[str, Any]:
    return {
        "type": "float",
        "default": round(value, 3),
        "min": minimum,
        "max": maximum,
    }


def _make_primitive_blueprint(primitive: str, material: str | None = None) -> dict[str, Any]:
    chosen_material = material or "wood"
    if primitive == "cylinder":
        return {
            "schema_version": 1,
            "id": "object_copilot_cylinder",
            "name": "Object Copilot Cylinder",
            "category": "prototype",
            "tags": ["prototype", "primitive", "cylinder"],
            "parameters": {
                "radius": _param_spec(40.0, 5.0, 300.0),
                "height": _param_spec(120.0, 10.0, 400.0),
            },
            "root": {
                "type": "primitive",
                "primitive": "cylinder",
                "params": {
                    "radius": "$radius",
                    "height": "$height",
                },
                "material": chosen_material,
            },
        }

    if primitive == "sphere":
        return {
            "schema_version": 1,
            "id": "object_copilot_sphere",
            "name": "Object Copilot Sphere",
            "category": "prototype",
            "tags": ["prototype", "primitive", "sphere"],
            "parameters": {
                "radius": _param_spec(60.0, 5.0, 300.0),
            },
            "root": {
                "type": "primitive",
                "primitive": "sphere",
                "params": {
                    "radius": "$radius",
                },
                "material": chosen_material,
            },
        }

    if primitive == "capsule":
        return {
            "schema_version": 1,
            "id": "object_copilot_capsule",
            "name": "Object Copilot Capsule",
            "category": "prototype",
            "tags": ["prototype", "primitive", "capsule"],
            "parameters": {
                "radius": _param_spec(30.0, 5.0, 200.0),
                "height": _param_spec(140.0, 20.0, 400.0),
            },
            "root": {
                "type": "primitive",
                "primitive": "capsule",
                "params": {
                    "radius": "$radius",
                    "height": "$height",
                },
                "material": chosen_material,
            },
        }

    return {
        "schema_version": 1,
        "id": "object_copilot_cube",
        "name": "Object Copilot Cube",
        "category": "prototype",
        "tags": ["prototype", "primitive", "cube"],
        "parameters": {
            "size": _param_spec(100.0, 10.0, 400.0),
        },
        "root": {
            "type": "primitive",
            "primitive": "cube",
            "params": {
                "size": "$size",
            },
            "material": chosen_material,
        },
    }


def _detect_requested_primitive(prompt: str) -> str | None:
    if _contains_any(prompt, ["cylinder", *ZH["cylinder"]]):
        return "cylinder"
    if _contains_any(prompt, ["sphere", *ZH["sphere"]]):
        return "sphere"
    if _contains_any(prompt, ["capsule", *ZH["capsule"]]):
        return "capsule"
    if _contains_any(prompt, ["cube", "box", *ZH["cube"]]):
        return "cube"
    return None


def _replace_with_requested_primitive(blueprint: dict[str, Any], primitive: str, notes: list[str]) -> dict[str, Any]:
    material = None
    root = blueprint.get("root")
    if isinstance(root, dict) and isinstance(root.get("material"), str):
        material = root.get("material")

    rebuilt = _make_primitive_blueprint(primitive, material)
    notes.append(f"Replaced the current object document with a generic '{primitive}' primitive blueprint.")
    return rebuilt


def _switch_root_primitive_in_place(blueprint: dict[str, Any], primitive: str, notes: list[str]) -> bool:
    root = blueprint.get("root")
    if not isinstance(root, dict) or root.get("type") != "primitive":
        return False

    root["primitive"] = primitive
    params = root.setdefault("params", {})
    if not isinstance(params, dict):
        params = {}
        root["params"] = params

    parameters = blueprint.setdefault("parameters", {})
    if not isinstance(parameters, dict):
        parameters = {}
        blueprint["parameters"] = parameters

    if primitive == "cylinder":
        parameters.setdefault("radius", _param_spec(40.0, 5.0, 300.0))
        parameters.setdefault("height", _param_spec(120.0, 10.0, 400.0))
        params.clear()
        params["radius"] = "$radius"
        params["height"] = "$height"
    elif primitive == "sphere":
        parameters.setdefault("radius", _param_spec(60.0, 5.0, 300.0))
        params.clear()
        params["radius"] = "$radius"
    elif primitive == "capsule":
        parameters.setdefault("radius", _param_spec(30.0, 5.0, 200.0))
        parameters.setdefault("height", _param_spec(140.0, 20.0, 400.0))
        params.clear()
        params["radius"] = "$radius"
        params["height"] = "$height"
    else:
        parameters.setdefault("size", _param_spec(100.0, 10.0, 400.0))
        params.clear()
        params["size"] = "$size"

    notes.append(f"Switched the root primitive to '{primitive}'.")
    return True


def _get_parameter_value(blueprint: dict[str, Any], name: str) -> float | None:
    parameters = blueprint.get("parameters")
    if not isinstance(parameters, dict) or name not in parameters:
        return None

    value = parameters[name]
    if isinstance(value, (int, float)):
        return float(value)
    if isinstance(value, dict) and isinstance(value.get("default"), (int, float)):
        return float(value["default"])
    return None


def _set_parameter_value(blueprint: dict[str, Any], name: str, value: float) -> bool:
    parameters = blueprint.get("parameters")
    if not isinstance(parameters, dict) or name not in parameters:
        return False

    current = parameters[name]
    if isinstance(current, (int, float)):
        parameters[name] = round(value, 3)
        return True
    if isinstance(current, dict):
        current["default"] = round(value, 3)
        return True
    return False


def _adjust_parameter(blueprint: dict[str, Any], name: str, delta: float) -> bool:
    current = _get_parameter_value(blueprint, name)
    if current is None:
        return False
    return _set_parameter_value(blueprint, name, current + delta)


def _find_first_parameter(blueprint: dict[str, Any], groups: list[list[str]]) -> str | None:
    parameters = blueprint.get("parameters")
    if not isinstance(parameters, dict):
        return None

    for names in groups:
        for name in names:
            if name in parameters:
                return name
    return None


def _update_materials(node: Any, material: str, count: list[int], limit: int = 4) -> None:
    if not isinstance(node, dict) or count[0] >= limit:
        return

    node_type = node.get("type")
    if node_type in {"primitive", "reference"} and count[0] < limit:
        node["material"] = material
        count[0] += 1

    if node_type == "group":
        for child in node.get("children", []):
            _update_materials(child, material, count, limit)
    elif node_type == "csg":
        _update_materials(node.get("left"), material, count, limit)
        _update_materials(node.get("right"), material, count, limit)


def _choose_preset(prompt: str, current_object_id: str) -> str | None:
    wants_new = (
        _contains_any(prompt, ["create ", "generate ", "new ", "start with ", "build a ", "make a "])
        or _contains_any(prompt, ZH["create"])
    )

    if current_object_id and current_object_id != "object_copilot_default_cube" and not wants_new:
        return None

    for preset_id, keywords in PRESET_KEYWORDS.items():
        if _contains_any(prompt, keywords):
            return preset_id
    return None


def _edit_document(blueprint: dict[str, Any], prompt: str, notes: list[str]) -> dict[str, Any]:
    current_id = str(blueprint.get("id") or blueprint.get("name") or "")
    preset_id = _choose_preset(prompt, current_id)
    if preset_id:
        loaded = _load_preset_blueprint(preset_id)
        if loaded is not None:
            blueprint = loaded
            notes.append(f"Loaded preset '{preset_id}' before applying the requested edits.")

    requested_primitive = _detect_requested_primitive(prompt)
    wants_new = _contains_any(prompt, ["create ", "generate ", "new ", "start with ", "build a ", "make a "]) or _contains_any(prompt, ZH["create"])
    if requested_primitive:
        root = blueprint.get("root")
        can_switch_in_place = isinstance(root, dict) and root.get("type") == "primitive"
        if wants_new or current_id == "object_copilot_default_cube":
            blueprint = _replace_with_requested_primitive(blueprint, requested_primitive, notes)
        elif not _switch_root_primitive_in_place(blueprint, requested_primitive, notes):
            blueprint = _replace_with_requested_primitive(blueprint, requested_primitive, notes)

    scale = 1.2
    if _contains_any(prompt, ["slightly", "a bit", "little", "\u7a0d\u5fae", "\u4e00\u70b9", "\u4e00\u70b9\u70b9"]):
        scale = 1.0
    elif _contains_any(prompt, ["much", "far more", "significantly", "\u5f88\u591a", "\u660e\u663e", "\u5927\u5e45"]):
        scale = 1.8

    bigger = _contains_any(prompt, ["bigger", "larger", "wider", "taller", "longer", "higher", "thicker", "raise", "raised", *ZH["bigger"], *ZH["higher"], *ZH["wider"], *ZH["longer"], *ZH["thicker"]])
    smaller = _contains_any(prompt, ["smaller", "shorter", "lower", "thinner", "narrower", *ZH["smaller"]])

    if bigger or smaller:
        sign = 1.0 if bigger else -1.0

        def adjust(groups: list[list[str]], base_delta: float, label: str) -> bool:
            target = _find_first_parameter(blueprint, groups)
            if not target:
                return False
            current = _get_parameter_value(blueprint, target) or (base_delta * 5.0)
            delta = max(base_delta, current * 0.08 * scale) * sign
            if _adjust_parameter(blueprint, target, delta):
                notes.append(f"Adjusted {label} by updating parameter '{target}'.")
                return True
            return False

        if _contains_any(prompt, ["wheel", "tire", "tyre", *ZH["wheel"]]):
            adjust([["radius"], ["wheel_radius"], ["r"]], 5.0, "wheel size")
        elif _contains_any(prompt, ["leg", *ZH["leg"]]):
            adjust([["leg_height"], ["h"], ["table_height"], ["seat_height"]], 6.0, "leg height")
        elif _contains_any(prompt, ["backrest", *ZH["backrest"]]):
            adjust([["backrest_height"]], 5.0, "backrest height")
        elif _contains_any(prompt, ["handle", "bar", "pole", *ZH["pole"]]):
            adjust([["pole_h"], ["handlebar_height"], ["arm_len"]], 4.0, "pole or handle height")
        elif _contains_any(prompt, ["shade", *ZH["shade"]]):
            adjust([["shade_r"], ["radius"]], 3.0, "shade size")
        elif _contains_any(prompt, ["tabletop", "top", "surface", *ZH["tabletop"]]):
            changed = adjust([["w"], ["table_width"], ["seat_width"], ["bed_width"]], 6.0, "surface width")
            changed = adjust([["d"], ["table_length"], ["table_depth"], ["seat_depth"], ["bed_length"]], 6.0, "surface depth") or changed
            if not changed:
                notes.append("Tried to enlarge the top surface, but no matching size parameters were found.")
        elif _contains_any(prompt, ["height", "higher", "taller", "raise", "raised", *ZH["higher"]]):
            adjust([["h"], ["height"], ["table_height"], ["pole_h"], ["leg_height"], ["seat_height"], ["backrest_height"], ["headboard_height"]], 5.0, "overall height")
        elif _contains_any(prompt, ["width", "wider", *ZH["wider"]]):
            adjust([["w"], ["width"], ["table_width"], ["seat_width"], ["bed_width"]], 5.0, "width")
        elif _contains_any(prompt, ["length", "longer", *ZH["longer"]]):
            adjust([["table_length"], ["bed_length"], ["arm_len"], ["length"], ["d"]], 6.0, "length")
        elif _contains_any(prompt, ["thick", "thickness", *ZH["thicker"]]):
            adjust([["t"], ["thickness"], ["frame_thickness"], ["shade_thickness"]], 2.0, "thickness")
        else:
            adjust([["size"], ["radius"], ["w"], ["table_length"]], 5.0, "size")

    if _contains_any(prompt, ["metal", "steel", "chrome", *ZH["metal"]]):
        material = "metal"
    elif _contains_any(prompt, ["wood", *ZH["wood"]]):
        material = "wood"
    elif _contains_any(prompt, ["glass", *ZH["glass"]]):
        material = "glass_tinted"
    elif _contains_any(prompt, ["plastic", *ZH["plastic"]]):
        material = "plastic"
    elif _contains_any(prompt, ["rubber", *ZH["rubber"]]):
        material = "rubber"
    else:
        material = None

    if material and material in ALLOWED_MATERIALS:
        root = blueprint.get("root")
        if isinstance(root, dict):
            applied = [0]
            _update_materials(root, material, applied)
            if applied[0] > 0:
                notes.append(f"Updated {applied[0]} node materials to '{material}'.")

    if not notes:
        notes.append("No stable document edit could be inferred from the current prompt.")

    return blueprint


def _document_rules() -> list[str]:
    return [
        "Treat the current object blueprint JSON as the single source of truth.",
        "Prefer minimal edits that keep unrelated parts unchanged.",
        "Prefer editing parameters before restructuring nodes.",
        "Keep schema_version, id, and the overall root structure stable unless the prompt clearly requests a new object.",
        "Use only supported material names from the object index.",
        "Map primitive shape words directly into geometry fields: cylinder -> root.primitive='cylinder', sphere -> 'sphere', capsule -> 'capsule', cube/box -> 'cube'.",
        "For a cylinder blueprint, prefer parameters named 'radius' and 'height' and bind root.params to those parameter names.",
        "For a sphere blueprint, prefer parameter 'radius'. For a cube blueprint, prefer parameter 'size'.",
    ]


@app.get("/health")
def health() -> dict:
    return {"ok": True, "service": "object-copilot-agent"}


@app.post("/agent/patch", response_model=AgentResponse)
def generate_patch(request: AgentRequest) -> AgentResponse:
    prompt = request.user_prompt.strip()
    if not prompt:
        return AgentResponse(
            summary="No prompt received.",
            updated_object_json=request.current_object_json or "{}",
            notes=["The service is running and waiting for a user prompt."],
        )

    blueprint = _parse_blueprint(request.current_object_json)
    notes = [f"Received prompt: {prompt}"]
    notes.extend(_document_rules())
    updated = _edit_document(blueprint, prompt, notes)
    summary = "Updated the current object document using the prompt and the document editing rules."
    return AgentResponse(
        summary=summary,
        updated_object_json=json.dumps(updated, indent=2, ensure_ascii=False),
        notes=notes,
    )

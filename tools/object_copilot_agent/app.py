from __future__ import annotations

import json
from pathlib import Path

from fastapi import FastAPI
from pydantic import BaseModel, Field


app = FastAPI(title="Moon Object Copilot Agent", version="0.1.0")

REPO_ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = REPO_ROOT / "assets" / "objects" / "components"

PRESET_FILES = {
    "table": ASSET_ROOT / "furniture" / "table_v1.json",
    "simple_table": ASSET_ROOT / "furniture" / "simple_table_v1.json",
    "chair": ASSET_ROOT / "furniture" / "chair_v1.json",
    "lamp": ASSET_ROOT / "furniture" / "desk_lamp_v1.json",
}


class AgentRequest(BaseModel):
    world_state: dict = Field(default_factory=dict)
    conversation: list[dict] = Field(default_factory=list)
    user_prompt: str = ""


class PatchOperation(BaseModel):
    op: str
    path: str
    value: object | None = None


class AgentResponse(BaseModel):
    summary: str
    operations: list[PatchOperation] = Field(default_factory=list)
    notes: list[str] = Field(default_factory=list)


def _replace(path: str, value: object) -> PatchOperation:
    return PatchOperation(op="replace", path=path, value=value)


def _maybe_add(path: str, value: object) -> PatchOperation:
    return PatchOperation(op="add", path=path, value=value)


def _contains_any(text: str, keywords: list[str]) -> bool:
    return any(keyword in text for keyword in keywords)


def _load_preset(name: str) -> dict:
    path = PRESET_FILES[name]
    return json.loads(path.read_text(encoding="utf-8"))


def _append_preset_operations(prompt: str, operations: list[PatchOperation], notes: list[str]) -> bool:
    lower = prompt.lower()

    if _contains_any(lower, ["table", "desk"]) or _contains_any(prompt, ["桌子", "餐桌", "书桌", "桌"]):
        operations.append(_replace("/object_blueprint", _load_preset("table")))
        notes.append("Switched the current object blueprint to the reusable table preset.")
        return True

    if _contains_any(lower, ["chair"]) or _contains_any(prompt, ["椅子", "凳子"]):
        operations.append(_replace("/object_blueprint", _load_preset("chair")))
        notes.append("Switched the current object blueprint to the chair preset.")
        return True

    if _contains_any(lower, ["lamp", "light"]) or _contains_any(prompt, ["台灯", "灯"]):
        operations.append(_replace("/object_blueprint", _load_preset("lamp")))
        notes.append("Switched the current object blueprint to the desk lamp preset.")
        return True

    return False


def _build_prompt_patch(prompt: str, world_state: dict) -> AgentResponse:
    lower = prompt.lower()
    blueprint = world_state.get("object_blueprint", {})
    operations: list[PatchOperation] = []
    notes: list[str] = []

    replaced_with_preset = _append_preset_operations(prompt, operations, notes)

    preview_blueprint = operations[-1].value if replaced_with_preset and operations else blueprint
    parameters = preview_blueprint.get("parameters", {}) if isinstance(preview_blueprint, dict) else {}
    root = preview_blueprint.get("root", {}) if isinstance(preview_blueprint, dict) else {}

    if _contains_any(lower, ["bigger", "larger"]) or _contains_any(prompt, ["更大", "放大", "大一点", "再大一点"]):
        if "size" in parameters:
            operations.append(_replace("/object_blueprint/parameters/size/default", 180.0))
            notes.append("Raised the default size parameter.")
        elif "w" in parameters:
            operations.append(_replace("/object_blueprint/parameters/w", 160.0))
            notes.append("Made the preset wider.")
        elif "table_length" in parameters:
            operations.append(_replace("/object_blueprint/parameters/table_length", 160.0))
            operations.append(_replace("/object_blueprint/parameters/table_width", 90.0))
            notes.append("Scaled the table top dimensions up.")

    if _contains_any(lower, ["smaller"]) or _contains_any(prompt, ["更小", "缩小", "小一点", "再小一点"]):
        if "size" in parameters:
            operations.append(_replace("/object_blueprint/parameters/size/default", 60.0))
            notes.append("Reduced the default size parameter.")
        elif "w" in parameters:
            operations.append(_replace("/object_blueprint/parameters/w", 90.0))
            notes.append("Made the preset narrower.")
        elif "table_length" in parameters:
            operations.append(_replace("/object_blueprint/parameters/table_length", 100.0))
            operations.append(_replace("/object_blueprint/parameters/table_width", 60.0))
            notes.append("Scaled the table top dimensions down.")

    if _contains_any(lower, ["metal"]) or _contains_any(prompt, ["金属"]):
        if isinstance(root, dict) and root.get("type") == "primitive":
            operations.append(_replace("/object_blueprint/root/material", "metal"))
        notes.append("Applied a metal material hint where possible.")

    if _contains_any(lower, ["wood"]) or _contains_any(prompt, ["木头", "木质", "木制"]):
        if isinstance(root, dict) and root.get("type") == "primitive":
            operations.append(_replace("/object_blueprint/root/material", "wood"))
        notes.append("Applied a wood material hint where possible.")

    if _contains_any(lower, ["glass"]) or _contains_any(prompt, ["玻璃"]):
        if isinstance(root, dict) and root.get("type") == "primitive":
            operations.append(_replace("/object_blueprint/root/material", "glass_tinted"))
        notes.append("Applied a glass material hint where possible.")

    if _contains_any(lower, ["sphere"]) or _contains_any(prompt, ["球", "球体"]):
        operations.append(_replace("/object_blueprint/root/primitive", "sphere"))
        operations.append(_replace("/object_blueprint/root/type", "primitive"))
        notes.append("Changed the object root to a sphere.")

    if _contains_any(lower, ["cylinder"]) or _contains_any(prompt, ["圆柱"]):
        operations.append(_replace("/object_blueprint/root/primitive", "cylinder"))
        operations.append(_replace("/object_blueprint/root/type", "primitive"))
        root_params = root.get("params", {}) if isinstance(root, dict) else {}
        if "radius" not in root_params:
            operations.append(_maybe_add("/object_blueprint/root/params/radius", 45.0))
        if "height" not in root_params:
            operations.append(_maybe_add("/object_blueprint/root/params/height", 140.0))
        notes.append("Changed the object root to a cylinder.")

    if _contains_any(lower, ["cone"]) or _contains_any(prompt, ["圆锥"]):
        operations.append(_replace("/object_blueprint/root/primitive", "cone"))
        operations.append(_replace("/object_blueprint/root/type", "primitive"))
        notes.append("Changed the object root to a cone.")

    if _contains_any(lower, ["red"]) or _contains_any(prompt, ["红", "红色"]):
        if isinstance(root, dict) and root.get("type") == "primitive":
            operations.append(_replace("/object_blueprint/root/material", "plastic"))
        notes.append("Used plastic as a red-friendly placeholder material.")

    if not operations:
        notes.append("No heuristic matched, so the current response is informational only.")

    summary = "Prepared a structured patch from the prompt."
    if notes:
        summary += " " + " ".join(notes[:2])

    return AgentResponse(summary=summary, operations=operations, notes=notes)


@app.get("/health")
def health() -> dict:
    return {"ok": True, "service": "object-copilot-agent"}


@app.post("/agent/patch", response_model=AgentResponse)
def generate_patch(request: AgentRequest) -> AgentResponse:
    prompt = request.user_prompt.strip()
    if not prompt:
        return AgentResponse(
            summary="No prompt received.",
            notes=["This scaffold keeps the API shape in place for the future agent loop."],
        )

    response = _build_prompt_patch(prompt, request.world_state)
    response.notes.insert(0, f"Received prompt: {prompt}")
    return response

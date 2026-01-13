/**
 * Moon Engine - Material Commands
 * 处理 Material 组件属性修改的命令
 */

import type { Command } from './Command';
import { engine } from '@/utils/engine-bridge';
import type { Vector3, MaterialPreset } from '@/types/engine';
import { useEditorStore } from '@/store/editorStore';

/**
 * SetMaterialPresetCommand - 设置材质预设（Glass, Metal, Rock 等）
 */
export class SetMaterialPresetCommand implements Command {
  private nodeId: number;
  private oldPreset: MaterialPreset;
  private newPreset: MaterialPreset;
  description: string;

  constructor(nodeId: number, oldPreset: MaterialPreset, newPreset: MaterialPreset) {
    this.nodeId = nodeId;
    this.oldPreset = oldPreset;
    this.newPreset = newPreset;
    this.description = `Set Material Preset of Node ${nodeId} to ${newPreset}`;
  }

  async execute(): Promise<void> {
    await engine.setMaterialPreset(this.nodeId, this.newPreset);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Material', {
      preset: this.newPreset
    });
  }

  async undo(): Promise<void> {
    await engine.setMaterialPreset(this.nodeId, this.oldPreset);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Material', {
      preset: this.oldPreset
    });
  }
}

/**
 * SetMaterialMetallicCommand - 设置材质金属度
 */
export class SetMaterialMetallicCommand implements Command {
  private nodeId: number;
  private oldMetallic: number;
  private newMetallic: number;
  description: string;

  constructor(nodeId: number, oldMetallic: number, newMetallic: number) {
    this.nodeId = nodeId;
    this.oldMetallic = oldMetallic;
    this.newMetallic = newMetallic;
    this.description = `Set Material Metallic of Node ${nodeId}`;
  }

  async execute(): Promise<void> {
    await engine.setMaterialMetallic(this.nodeId, this.newMetallic);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Material', {
      metallic: this.newMetallic
    });
  }

  async undo(): Promise<void> {
    await engine.setMaterialMetallic(this.nodeId, this.oldMetallic);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Material', {
      metallic: this.oldMetallic
    });
  }
}

/**
 * SetMaterialRoughnessCommand - 设置材质粗糙度
 */
export class SetMaterialRoughnessCommand implements Command {
  private nodeId: number;
  private oldRoughness: number;
  private newRoughness: number;
  description: string;

  constructor(nodeId: number, oldRoughness: number, newRoughness: number) {
    this.nodeId = nodeId;
    this.oldRoughness = oldRoughness;
    this.newRoughness = newRoughness;
    this.description = `Set Material Roughness of Node ${nodeId}`;
  }

  async execute(): Promise<void> {
    await engine.setMaterialRoughness(this.nodeId, this.newRoughness);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Material', {
      roughness: this.newRoughness
    });
  }

  async undo(): Promise<void> {
    await engine.setMaterialRoughness(this.nodeId, this.oldRoughness);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Material', {
      roughness: this.oldRoughness
    });
  }
}

/**
 * SetMaterialBaseColorCommand - 设置材质基础颜色
 */
export class SetMaterialBaseColorCommand implements Command {
  private nodeId: number;
  private oldBaseColor: Vector3;
  private newBaseColor: Vector3;
  description: string;

  constructor(nodeId: number, oldBaseColor: Vector3 | number[], newBaseColor: Vector3 | number[]) {
    this.nodeId = nodeId;
    // 支持数组和 Vector3 两种格式
    this.oldBaseColor = Array.isArray(oldBaseColor) 
      ? { x: oldBaseColor[0], y: oldBaseColor[1], z: oldBaseColor[2] }
      : oldBaseColor;
    this.newBaseColor = Array.isArray(newBaseColor)
      ? { x: newBaseColor[0], y: newBaseColor[1], z: newBaseColor[2] }
      : newBaseColor;
    this.description = `Set Material Base Color of Node ${nodeId}`;
  }

  async execute(): Promise<void> {
    await engine.setMaterialBaseColor(this.nodeId, this.newBaseColor);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Material', {
      baseColor: [this.newBaseColor.x, this.newBaseColor.y, this.newBaseColor.z]
    });
  }

  async undo(): Promise<void> {
    await engine.setMaterialBaseColor(this.nodeId, this.oldBaseColor);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Material', {
      baseColor: [this.oldBaseColor.x, this.oldBaseColor.y, this.oldBaseColor.z]
    });
  }
}


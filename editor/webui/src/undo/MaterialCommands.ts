/**
 * Moon Engine - Material Commands
 * 处理 Material 组件属性修改的命令
 */

import type { Command } from './Command';
import { engine } from '@/utils/engine-bridge';
import type { Vector3 } from '@/types/engine';
import { useEditorStore } from '@/store/editorStore';

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

  constructor(nodeId: number, oldBaseColor: Vector3, newBaseColor: Vector3) {
    this.nodeId = nodeId;
    this.oldBaseColor = oldBaseColor;
    this.newBaseColor = newBaseColor;
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

/**
 * SetMaterialTextureCommand - 设置材质贴图
 */
export class SetMaterialTextureCommand implements Command {
  private nodeId: number;
  private textureType: 'albedo' | 'normal' | 'arm';
  private oldPath: string;
  private newPath: string;
  description: string;

  constructor(nodeId: number, textureType: 'albedo' | 'normal' | 'arm', oldPath: string, newPath: string) {
    this.nodeId = nodeId;
    this.textureType = textureType;
    this.oldPath = oldPath;
    this.newPath = newPath;
    this.description = `Set Material ${textureType} Texture of Node ${nodeId}`;
  }

  async execute(): Promise<void> {
    await engine.setMaterialTexture(this.nodeId, this.textureType, this.newPath);
    
    const updateKey = this.textureType === 'albedo' ? 'albedoMap' : 
                      this.textureType === 'normal' ? 'normalMap' : 'armMap';
    
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Material', {
      [updateKey]: this.newPath
    });
  }

  async undo(): Promise<void> {
    await engine.setMaterialTexture(this.nodeId, this.textureType, this.oldPath);
    
    const updateKey = this.textureType === 'albedo' ? 'albedoMap' : 
                      this.textureType === 'normal' ? 'normalMap' : 'armMap';
    
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Material', {
      [updateKey]: this.oldPath
    });
  }
}

/**
 * Moon Engine - Skybox Commands
 * 处理 Skybox 组件属性修改的命令
 */

import type { Command } from './Command';
import { engine } from '@/utils/engine-bridge';
import type { Vector3 } from '@/types/engine';
import { useEditorStore } from '@/store/editorStore';

/**
 * SetSkyboxIntensityCommand - 设置天空盒亮度
 */
export class SetSkyboxIntensityCommand implements Command {
  private nodeId: number;
  private oldIntensity: number;
  private newIntensity: number;
  description: string;

  constructor(nodeId: number, oldIntensity: number, newIntensity: number) {
    this.nodeId = nodeId;
    this.oldIntensity = oldIntensity;
    this.newIntensity = newIntensity;
    this.description = `Set Skybox Intensity`;
  }

  async execute(): Promise<void> {
    await engine.setSkyboxIntensity(this.nodeId, this.newIntensity);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Skybox', {
      intensity: this.newIntensity
    });
  }

  async undo(): Promise<void> {
    await engine.setSkyboxIntensity(this.nodeId, this.oldIntensity);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Skybox', {
      intensity: this.oldIntensity
    });
  }
}

/**
 * SetSkyboxRotationCommand - 设置天空盒旋转
 */
export class SetSkyboxRotationCommand implements Command {
  private nodeId: number;
  private oldRotation: number;
  private newRotation: number;
  description: string;

  constructor(nodeId: number, oldRotation: number, newRotation: number) {
    this.nodeId = nodeId;
    this.oldRotation = oldRotation;
    this.newRotation = newRotation;
    this.description = `Set Skybox Rotation`;
  }

  async execute(): Promise<void> {
    await engine.setSkyboxRotation(this.nodeId, this.newRotation);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Skybox', {
      rotation: this.newRotation
    });
  }

  async undo(): Promise<void> {
    await engine.setSkyboxRotation(this.nodeId, this.oldRotation);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Skybox', {
      rotation: this.oldRotation
    });
  }
}

/**
 * SetSkyboxTintCommand - 设置天空盒色调
 */
export class SetSkyboxTintCommand implements Command {
  private nodeId: number;
  private oldTint: Vector3;
  private newTint: Vector3;
  description: string;

  constructor(nodeId: number, oldTint: Vector3, newTint: Vector3) {
    this.nodeId = nodeId;
    this.oldTint = oldTint;
    this.newTint = newTint;
    this.description = `Set Skybox Tint`;
  }

  async execute(): Promise<void> {
    await engine.setSkyboxTint(this.nodeId, this.newTint);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Skybox', {
      tint: [this.newTint.x, this.newTint.y, this.newTint.z]
    });
  }

  async undo(): Promise<void> {
    await engine.setSkyboxTint(this.nodeId, this.oldTint);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Skybox', {
      tint: [this.oldTint.x, this.oldTint.y, this.oldTint.z]
    });
  }
}

/**
 * SetSkyboxIBLCommand - 设置是否启用 IBL
 */
export class SetSkyboxIBLCommand implements Command {
  private nodeId: number;
  private oldEnableIBL: boolean;
  private newEnableIBL: boolean;
  description: string;

  constructor(nodeId: number, oldEnableIBL: boolean, newEnableIBL: boolean) {
    this.nodeId = nodeId;
    this.oldEnableIBL = oldEnableIBL;
    this.newEnableIBL = newEnableIBL;
    this.description = `Set Skybox IBL ${newEnableIBL ? 'On' : 'Off'}`;
  }

  async execute(): Promise<void> {
    await engine.setSkyboxIBL(this.nodeId, this.newEnableIBL);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Skybox', {
      enableIBL: this.newEnableIBL
    });
  }

  async undo(): Promise<void> {
    await engine.setSkyboxIBL(this.nodeId, this.oldEnableIBL);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Skybox', {
      enableIBL: this.oldEnableIBL
    });
  }
}

/**
 * SetSkyboxEnvironmentMapCommand - 设置环境贴图路径
 */
export class SetSkyboxEnvironmentMapCommand implements Command {
  private nodeId: number;
  private oldPath: string;
  private newPath: string;
  description: string;

  constructor(nodeId: number, oldPath: string, newPath: string) {
    this.nodeId = nodeId;
    this.oldPath = oldPath;
    this.newPath = newPath;
    this.description = `Set Skybox Environment Map`;
  }

  async execute(): Promise<void> {
    await engine.setSkyboxEnvironmentMap(this.nodeId, this.newPath);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Skybox', {
      environmentMapPath: this.newPath
    });
  }

  async undo(): Promise<void> {
    await engine.setSkyboxEnvironmentMap(this.nodeId, this.oldPath);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Skybox', {
      environmentMapPath: this.oldPath
    });
  }
}

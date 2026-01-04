/**
 * Moon Engine - Light Commands
 * 处理 Light 组件属性修改的命令
 */

import type { Command } from './Command';
import { engine } from '@/utils/engine-bridge';
import type { Vector3 } from '@/types/engine';
import { useEditorStore } from '@/store/editorStore';

/**
 * SetLightColorCommand - 设置光源颜色
 */
export class SetLightColorCommand implements Command {
  private nodeId: number;
  private oldColor: Vector3;
  private newColor: Vector3;
  description: string;

  constructor(nodeId: number, oldColor: Vector3, newColor: Vector3) {
    this.nodeId = nodeId;
    this.oldColor = oldColor;
    this.newColor = newColor;
    this.description = `Set Light Color`;
  }

  async execute(): Promise<void> {
    await engine.setLightColor(this.nodeId, this.newColor);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Light', {
      color: [this.newColor.x, this.newColor.y, this.newColor.z]
    });
  }

  async undo(): Promise<void> {
    await engine.setLightColor(this.nodeId, this.oldColor);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Light', {
      color: [this.oldColor.x, this.oldColor.y, this.oldColor.z]
    });
  }
}

/**
 * SetLightIntensityCommand - 设置光源强度
 */
export class SetLightIntensityCommand implements Command {
  private nodeId: number;
  private oldIntensity: number;
  private newIntensity: number;
  description: string;

  constructor(nodeId: number, oldIntensity: number, newIntensity: number) {
    this.nodeId = nodeId;
    this.oldIntensity = oldIntensity;
    this.newIntensity = newIntensity;
    this.description = `Set Light Intensity`;
  }

  async execute(): Promise<void> {
    await engine.setLightIntensity(this.nodeId, this.newIntensity);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Light', {
      intensity: this.newIntensity
    });
  }

  async undo(): Promise<void> {
    await engine.setLightIntensity(this.nodeId, this.oldIntensity);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Light', {
      intensity: this.oldIntensity
    });
  }
}

/**
 * SetLightRangeCommand - 设置光源范围（Point/Spot）
 */
export class SetLightRangeCommand implements Command {
  private nodeId: number;
  private oldRange: number;
  private newRange: number;
  description: string;

  constructor(nodeId: number, oldRange: number, newRange: number) {
    this.nodeId = nodeId;
    this.oldRange = oldRange;
    this.newRange = newRange;
    this.description = `Set Light Range`;
  }

  async execute(): Promise<void> {
    await engine.setLightRange(this.nodeId, this.newRange);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Light', {
      range: this.newRange
    });
  }

  async undo(): Promise<void> {
    await engine.setLightRange(this.nodeId, this.oldRange);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Light', {
      range: this.oldRange
    });
  }
}

/**
 * SetLightTypeCommand - 设置光源类型
 */
export class SetLightTypeCommand implements Command {
  private nodeId: number;
  private oldType: 'Directional' | 'Point' | 'Spot';
  private newType: 'Directional' | 'Point' | 'Spot';
  description: string;

  constructor(nodeId: number, oldType: 'Directional' | 'Point' | 'Spot', newType: 'Directional' | 'Point' | 'Spot') {
    this.nodeId = nodeId;
    this.oldType = oldType;
    this.newType = newType;
    this.description = `Set Light Type to ${newType}`;
  }

  async execute(): Promise<void> {
    await engine.setLightType(this.nodeId, this.newType);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Light', {
      lightType: this.newType
    });
  }

  async undo(): Promise<void> {
    await engine.setLightType(this.nodeId, this.oldType);
    useEditorStore.getState().updateNodeComponent(this.nodeId, 'Light', {
      lightType: this.oldType
    });
  }
}

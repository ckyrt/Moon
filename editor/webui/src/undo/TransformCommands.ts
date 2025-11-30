/**
 * Moon Engine - Transform Commands
 * 处理 Position、Rotation、Scale 的修改命令
 */

import type { Command } from './Command';
import type { Vector3, Quaternion } from '@/types/engine';
import { engine } from '@/utils/engine-bridge';
import { useEditorStore } from '@/store/editorStore';

/**
 * SetPositionCommand - 设置节点位置
 */
export class SetPositionCommand implements Command {
  private nodeId: number;
  private oldPosition: Vector3;
  private newPosition: Vector3;
  description: string;

  constructor(nodeId: number, oldPosition: Vector3, newPosition: Vector3) {
    this.nodeId = nodeId;
    this.oldPosition = { ...oldPosition };
    this.newPosition = { ...newPosition };
    this.description = `Set Position of Node ${nodeId}`;
  }

  execute(): void {
    // 1. 更新引擎
    engine.setPosition(this.nodeId, this.newPosition);

    // 2. 更新 UI Store
    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      position: this.newPosition,
    });
  }

  undo(): void {
    // 1. 恢复引擎
    engine.setPosition(this.nodeId, this.oldPosition);

    // 2. 恢复 UI Store
    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      position: this.oldPosition,
    });
  }
}

/**
 * SetRotationCommand - 设置节点旋转
 */
export class SetRotationCommand implements Command {
  private nodeId: number;
  private oldRotation: Vector3; // 欧拉角（用于 UI 显示）
  private newRotation: Vector3;
  private oldQuaternion: Quaternion; // 四元数（用于引擎）
  private newQuaternion: Quaternion;
  description: string;

  constructor(
    nodeId: number,
    oldRotation: Vector3,
    newRotation: Vector3,
    oldQuaternion: Quaternion,
    newQuaternion: Quaternion
  ) {
    this.nodeId = nodeId;
    this.oldRotation = { ...oldRotation };
    this.newRotation = { ...newRotation };
    this.oldQuaternion = { ...oldQuaternion };
    this.newQuaternion = { ...newQuaternion };
    this.description = `Set Rotation of Node ${nodeId}`;
  }

  execute(): void {
    // 1. 更新引擎（使用四元数）
    engine.setRotation(this.nodeId, this.newQuaternion);

    // 2. 更新 UI Store（使用欧拉角）
    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      rotation: this.newRotation,
    });
  }

  undo(): void {
    // 1. 恢复引擎
    engine.setRotation(this.nodeId, this.oldQuaternion);

    // 2. 恢复 UI Store
    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      rotation: this.oldRotation,
    });
  }
}

/**
 * SetScaleCommand - 设置节点缩放
 */
export class SetScaleCommand implements Command {
  private nodeId: number;
  private oldScale: Vector3;
  private newScale: Vector3;
  description: string;

  constructor(nodeId: number, oldScale: Vector3, newScale: Vector3) {
    this.nodeId = nodeId;
    this.oldScale = { ...oldScale };
    this.newScale = { ...newScale };
    this.description = `Set Scale of Node ${nodeId}`;
  }

  execute(): void {
    // 1. 更新引擎
    engine.setScale(this.nodeId, this.newScale);

    // 2. 更新 UI Store
    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      scale: this.newScale,
    });
  }

  undo(): void {
    // 1. 恢复引擎
    engine.setScale(this.nodeId, this.oldScale);

    // 2. 恢复 UI Store
    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      scale: this.oldScale,
    });
  }
}

/**
 * SetTransformCommand - 同时设置 Position、Rotation、Scale
 * （用于 Gizmo 拖拽结束时的批量更新）
 */
export class SetTransformCommand implements Command {
  private nodeId: number;
  private oldPosition: Vector3;
  private newPosition: Vector3;
  private oldRotation: Vector3;
  private newRotation: Vector3;
  private oldScale: Vector3;
  private newScale: Vector3;
  private oldQuaternion: Quaternion;
  private newQuaternion: Quaternion;
  description: string;

  constructor(
    nodeId: number,
    oldTransform: {
      position: Vector3;
      rotation: Vector3;
      scale: Vector3;
      quaternion: Quaternion;
    },
    newTransform: {
      position: Vector3;
      rotation: Vector3;
      scale: Vector3;
      quaternion: Quaternion;
    }
  ) {
    this.nodeId = nodeId;
    this.oldPosition = { ...oldTransform.position };
    this.newPosition = { ...newTransform.position };
    this.oldRotation = { ...oldTransform.rotation };
    this.newRotation = { ...newTransform.rotation };
    this.oldScale = { ...oldTransform.scale };
    this.newScale = { ...newTransform.scale };
    this.oldQuaternion = { ...oldTransform.quaternion };
    this.newQuaternion = { ...newTransform.quaternion };
    this.description = `Set Transform of Node ${nodeId}`;
  }

  execute(): void {
    engine.setPosition(this.nodeId, this.newPosition);
    engine.setRotation(this.nodeId, this.newQuaternion);
    engine.setScale(this.nodeId, this.newScale);

    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      position: this.newPosition,
      rotation: this.newRotation,
      scale: this.newScale,
    });
  }

  undo(): void {
    engine.setPosition(this.nodeId, this.oldPosition);
    engine.setRotation(this.nodeId, this.oldQuaternion);
    engine.setScale(this.nodeId, this.oldScale);

    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      position: this.oldPosition,
      rotation: this.oldRotation,
      scale: this.oldScale,
    });
  }
}

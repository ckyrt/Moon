/**
 * Transform Commands - Position/Rotation/Scale modifications
 */

import type { Command } from './Command';
import type { Vector3, Quaternion } from '@/types/engine';
import { engine } from '@/utils/engine-bridge';
import { useEditorStore } from '@/store/editorStore';

export class SetPositionCommand implements Command {
  description: string;

  constructor(
    private nodeId: number,
    private oldPosition: Vector3,
    private newPosition: Vector3
  ) {
    this.oldPosition = { ...oldPosition };
    this.newPosition = { ...newPosition };
    this.description = `Set Position of Node ${nodeId}`;
  }

  execute(): void {
    engine.setPosition(this.nodeId, this.newPosition);
    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      position: this.newPosition,
    });
  }

  undo(): void {
    engine.setPosition(this.nodeId, this.oldPosition);
    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      position: this.oldPosition,
    });
  }
}

export class SetRotationCommand implements Command {
  description: string;

  constructor(
    private nodeId: number,
    private oldRotation: Vector3,
    private newRotation: Vector3,
    private oldQuaternion: Quaternion,
    private newQuaternion: Quaternion
  ) {
    this.oldRotation = { ...oldRotation };
    this.newRotation = { ...newRotation };
    this.oldQuaternion = { ...oldQuaternion };
    this.newQuaternion = { ...newQuaternion };
    this.description = `Set Rotation of Node ${nodeId}`;
  }

  execute(): void {
    engine.setRotation(this.nodeId, this.newQuaternion);
    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      rotation: this.newRotation,
    });
  }

  undo(): void {
    engine.setRotation(this.nodeId, this.oldQuaternion);
    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      rotation: this.oldRotation,
    });
  }
}

export class SetScaleCommand implements Command {
  description: string;

  constructor(
    private nodeId: number,
    private oldScale: Vector3,
    private newScale: Vector3
  ) {
    this.oldScale = { ...oldScale };
    this.newScale = { ...newScale };
    this.description = `Set Scale of Node ${nodeId}`;
  }

  execute(): void {
    engine.setScale(this.nodeId, this.newScale);
    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      scale: this.newScale,
    });
  }

  undo(): void {
    engine.setScale(this.nodeId, this.oldScale);
    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      scale: this.oldScale,
    });
  }
}

/**
 * Batch transform update (used by Gizmo)
 */
export class SetTransformCommand implements Command {
  description: string;

  constructor(
    private nodeId: number,
    private oldTransform: {
      position: Vector3;
      rotation: Vector3;
      scale: Vector3;
      quaternion: Quaternion;
    },
    private newTransform: {
      position: Vector3;
      rotation: Vector3;
      scale: Vector3;
      quaternion: Quaternion;
    }
  ) {
    this.oldTransform = {
      position: { ...oldTransform.position },
      rotation: { ...oldTransform.rotation },
      scale: { ...oldTransform.scale },
      quaternion: { ...oldTransform.quaternion },
    };
    this.newTransform = {
      position: { ...newTransform.position },
      rotation: { ...newTransform.rotation },
      scale: { ...newTransform.scale },
      quaternion: { ...newTransform.quaternion },
    };
    this.description = `Set Transform of Node ${nodeId}`;
  }

  execute(): void {
    engine.setPosition(this.nodeId, this.newTransform.position);
    engine.setRotation(this.nodeId, this.newTransform.quaternion);
    engine.setScale(this.nodeId, this.newTransform.scale);

    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      position: this.newTransform.position,
      rotation: this.newTransform.rotation,
      scale: this.newTransform.scale,
    });
  }

  undo(): void {
    engine.setPosition(this.nodeId, this.oldTransform.position);
    engine.setRotation(this.nodeId, this.oldTransform.quaternion);
    engine.setScale(this.nodeId, this.oldTransform.scale);

    useEditorStore.getState().updateNodeTransform(this.nodeId, {
      position: this.oldTransform.position,
      rotation: this.oldTransform.rotation,
      scale: this.oldTransform.scale,
    });
  }
}

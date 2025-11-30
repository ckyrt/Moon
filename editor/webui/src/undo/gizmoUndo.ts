/**
 * Gizmo Undo Integration
 * 
 * C++ calls onGizmoStart/onGizmoEnd, JS records state and creates undo commands
 */

import { getUndoManager } from '@/undo';
import { SetTransformCommand } from '@/undo';
import type { Vector3, Quaternion } from '@/types/engine';
import { useEditorStore } from '@/store/editorStore';
import { eulerToQuaternion } from '@/utils/math';

interface GizmoTransformState {
  nodeId: number;
  startPosition: Vector3;
  startRotation: Vector3;
  startScale: Vector3;
  startQuaternion: Quaternion;
}

let gizmoStartState: GizmoTransformState | null = null;

export function onGizmoStart(nodeId: number): void {
  const scene = useEditorStore.getState().scene;
  const node = scene.allNodes[nodeId];
  
  if (!node) {
    console.warn('[GizmoUndo] Node not found:', nodeId);
    return;
  }

  const quaternion = eulerToQuaternion(node.transform.rotation);

  gizmoStartState = {
    nodeId,
    startPosition: { ...node.transform.position },
    startRotation: { ...node.transform.rotation },
    startScale: { ...node.transform.scale },
    startQuaternion: quaternion,
  };

  console.log('[GizmoUndo] Drag started', gizmoStartState);
}

export function onGizmoEnd(
  nodeId: number,
  endPosition: Vector3,
  endRotation: Vector3,
  endScale: Vector3,
  endQuaternion: Quaternion
): void {
  if (!gizmoStartState || gizmoStartState.nodeId !== nodeId) {
    console.warn('[GizmoUndo] No start state:', nodeId);
    return;
  }

  const hasChanged = 
    !vectorEquals(gizmoStartState.startPosition, endPosition) ||
    !vectorEquals(gizmoStartState.startRotation, endRotation) ||
    !vectorEquals(gizmoStartState.startScale, endScale);

  if (!hasChanged) {
    console.log('[GizmoUndo] No transform change, skipping');
    gizmoStartState = null;
    return;
  }

  const command = new SetTransformCommand(
    nodeId,
    {
      position: gizmoStartState.startPosition,
      rotation: gizmoStartState.startRotation,
      scale: gizmoStartState.startScale,
      quaternion: gizmoStartState.startQuaternion,
    },
    {
      position: endPosition,
      rotation: endRotation,
      scale: endScale,
      quaternion: endQuaternion,
    }
  );

  // Update UI store (C++ already modified engine)
  useEditorStore.getState().updateNodeTransform(nodeId, {
    position: endPosition,
    rotation: endRotation,
    scale: endScale,
  });

  // Push without execute (transform already applied by C++)
  const undoManager = getUndoManager();
  undoManager.pushExecutedCommand(command);

  console.log('[GizmoUndo] Command recorded', command);

  gizmoStartState = null;
}

function vectorEquals(a: Vector3, b: Vector3, epsilon = 0.0001): boolean {
  return (
    Math.abs(a.x - b.x) < epsilon &&
    Math.abs(a.y - b.y) < epsilon &&
    Math.abs(a.z - b.z) < epsilon
  );
}

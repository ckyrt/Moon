/**
 * Moon Engine - Gizmo Undo Integration
 * 处理 Gizmo 拖拽时的 Undo/Redo 记录
 */

import { getUndoManager } from '@/undo';
import { SetTransformCommand } from '@/undo';
import type { Vector3, Quaternion } from '@/types/engine';
import { useEditorStore } from '@/store/editorStore';
import { eulerToQuaternion } from '@/utils/math';

/**
 * Gizmo 拖拽状态跟踪
 */
interface GizmoTransformState {
  nodeId: number;
  startPosition: Vector3;
  startRotation: Vector3;
  startScale: Vector3;
  startQuaternion: Quaternion;
}

let gizmoStartState: GizmoTransformState | null = null;

/**
 * 当 Gizmo 开始拖拽时调用（记录初始状态）
 */
export function onGizmoStart(nodeId: number): void {
  const scene = useEditorStore.getState().scene;
  const node = scene.allNodes[nodeId];
  
  if (!node) {
    console.warn('[GizmoUndo] onGizmoStart: node not found', nodeId);
    return;
  }

  // 将欧拉角转换为四元数
  const quaternion = eulerToQuaternion(node.transform.rotation);

  // 记录初始状态
  gizmoStartState = {
    nodeId,
    startPosition: { ...node.transform.position },
    startRotation: { ...node.transform.rotation },
    startScale: { ...node.transform.scale },
    startQuaternion: quaternion,
  };

  console.log('[GizmoUndo] Gizmo drag started', gizmoStartState);
}

/**
 * 当 Gizmo 拖拽结束时调用（创建 Command）
 */
export function onGizmoEnd(
  nodeId: number,
  endPosition: Vector3,
  endRotation: Vector3,
  endScale: Vector3,
  endQuaternion: Quaternion
): void {
  if (!gizmoStartState || gizmoStartState.nodeId !== nodeId) {
    console.warn('[GizmoUndo] onGizmoEnd: no start state', nodeId);
    return;
  }

  // 检查是否真的有变化
  const hasChanged = 
    !vectorEquals(gizmoStartState.startPosition, endPosition) ||
    !vectorEquals(gizmoStartState.startRotation, endRotation) ||
    !vectorEquals(gizmoStartState.startScale, endScale);

  if (!hasChanged) {
    console.log('[GizmoUndo] No transform change detected, skipping undo record');
    gizmoStartState = null;
    return;
  }

  // 创建 SetTransformCommand
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

  // 🎯 更新 UI Store（因为 Gizmo 已经修改了引擎，但 UI 还是旧数据）
  useEditorStore.getState().updateNodeTransform(nodeId, {
    position: endPosition,
    rotation: endRotation,
    scale: endScale,
  });

  // 注意：这里不调用 execute()，因为 Transform 已经被 Gizmo 修改了
  // 我们使用 pushExecutedCommand() 将命令加入栈，以便支持 Undo
  const undoManager = getUndoManager();
  undoManager.pushExecutedCommand(command);

  console.log('[GizmoUndo] Transform command recorded and UI updated', command);

  // 清空状态
  gizmoStartState = null;
}

/**
 * 辅助函数：比较两个 Vector3 是否相等
 */
function vectorEquals(a: Vector3, b: Vector3, epsilon = 0.0001): boolean {
  return (
    Math.abs(a.x - b.x) < epsilon &&
    Math.abs(a.y - b.y) < epsilon &&
    Math.abs(a.z - b.z) < epsilon
  );
}

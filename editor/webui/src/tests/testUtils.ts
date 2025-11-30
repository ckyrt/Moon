/**
 * Moon Engine - 测试工具函数
 * 提供断言和测试辅助功能
 */

import type { SceneNode } from '../types/engine';

/**
 * 断言两个值相等
 */
export function assertEquals(actual: any, expected: any, message: string): void {
  if (actual !== expected) {
    const error = `❌ 断言失败: ${message}\n   期望: ${expected}\n   实际: ${actual}`;
    console.error(error);
    throw new Error(error);
  }
  console.log(`✅ 断言通过: ${message}`);
}

/**
 * 断言 Vector3 相等
 */
export function assertVector3Equals(
  actual: { x: number; y: number; z: number },
  expected: { x: number; y: number; z: number },
  message: string
): void {
  if (actual.x !== expected.x || actual.y !== expected.y || actual.z !== expected.z) {
    const error = `❌ 断言失败: ${message}\n   期望: (${expected.x}, ${expected.y}, ${expected.z})\n   实际: (${actual.x}, ${actual.y}, ${actual.z})`;
    console.error(error);
    throw new Error(error);
  }
  console.log(`✅ 断言通过: ${message} = (${actual.x}, ${actual.y}, ${actual.z})`);
}

/**
 * 断言 Vector3 近似相等（浮点数容差）
 */
export function assertVector3Near(
  actual: { x: number; y: number; z: number },
  expected: { x: number; y: number; z: number },
  message: string,
  epsilon: number = 0.0001
): void {
  const deltaX = Math.abs(actual.x - expected.x);
  const deltaY = Math.abs(actual.y - expected.y);
  const deltaZ = Math.abs(actual.z - expected.z);
  
  if (deltaX > epsilon || deltaY > epsilon || deltaZ > epsilon) {
    const error = `❌ 断言失败: ${message}\n   期望: (${expected.x}, ${expected.y}, ${expected.z})\n   实际: (${actual.x}, ${actual.y}, ${actual.z})\n   差值: (${deltaX}, ${deltaY}, ${deltaZ}) > ${epsilon}`;
    console.error(error);
    throw new Error(error);
  }
  console.log(`✅ 断言通过: ${message} ≈ (${actual.x.toFixed(4)}, ${actual.y.toFixed(4)}, ${actual.z.toFixed(4)})`);
}

/**
 * 断言节点存在
 */
export function assertNodeExists(
  nodeId: number,
  scene: { allNodes: { [id: number]: SceneNode } },
  message: string
): void {
  if (!scene.allNodes[nodeId]) {
    const error = `❌ 断言失败: ${message} - 节点 ${nodeId} 不存在`;
    console.error(error);
    throw new Error(error);
  }
  console.log(`✅ 断言通过: ${message}`);
}

/**
 * 断言节点不存在
 */
export function assertNodeNotExists(
  nodeId: number,
  scene: { allNodes: { [id: number]: SceneNode } },
  message: string
): void {
  if (scene.allNodes[nodeId]) {
    const error = `❌ 断言失败: ${message} - 节点 ${nodeId} 应该不存在但实际存在`;
    console.error(error);
    throw new Error(error);
  }
  console.log(`✅ 断言通过: ${message}`);
}

/**
 * 断言条件为真
 */
export function assertTrue(condition: boolean, message: string): void {
  if (!condition) {
    const error = `❌ 断言失败: ${message}`;
    console.error(error);
    throw new Error(error);
  }
  console.log(`✅ 断言通过: ${message}`);
}

/**
 * 断言条件为假
 */
export function assertFalse(condition: boolean, message: string): void {
  if (condition) {
    const error = `❌ 断言失败: ${message}`;
    console.error(error);
    throw new Error(error);
  }
  console.log(`✅ 断言通过: ${message}`);
}

/**
 * 断言值不为 null 或 undefined
 */
export function assertNotNull<T>(value: T | null | undefined, message: string): asserts value is T {
  if (value === null || value === undefined) {
    const error = `❌ 断言失败: ${message} - 值为 ${value}`;
    console.error(error);
    throw new Error(error);
  }
  console.log(`✅ 断言通过: ${message}`);
}

/**
 * 断言数组长度
 */
export function assertArrayLength(array: any[], expectedLength: number, message: string): void {
  if (array.length !== expectedLength) {
    const error = `❌ 断言失败: ${message}\n   期望长度: ${expectedLength}\n   实际长度: ${array.length}`;
    console.error(error);
    throw new Error(error);
  }
  console.log(`✅ 断言通过: ${message} (长度=${array.length})`);
}

/**
 * 断言数组包含元素
 */
export function assertArrayContains<T>(array: T[], element: T, message: string): void {
  if (!array.includes(element)) {
    const error = `❌ 断言失败: ${message}\n   数组不包含元素: ${element}`;
    console.error(error);
    throw new Error(error);
  }
  console.log(`✅ 断言通过: ${message}`);
}

/**
 * 辅助函数：休眠指定毫秒
 */
export async function sleep(ms: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

/**
 * 打印场景信息（用于调试）
 */
export function printScene(
  label: string,
  scene: { allNodes: { [id: number]: SceneNode } }
): void {
  const nodes = scene?.allNodes || {};
  const nodeArray = Object.values(nodes);
  
  console.log(`\n🔍 === ${label} ===`);
  console.log(`节点总数: ${nodeArray.length}`);
  
  if (nodeArray.length === 0) {
    console.log('场景为空');
  } else {
    nodeArray.forEach((n: SceneNode) => {
      const parent = n.parentId ? nodes[n.parentId] : null;
      const parentInfo = parent ? ` [parent: ${parent.name}(${n.parentId})]` : ' [root]';
      const pos = n.transform.position;
      const posInfo = ` pos:(${pos.x.toFixed(1)}, ${pos.y.toFixed(1)}, ${pos.z.toFixed(1)})`;
      console.log(`  - ${n.name}(id:${n.id})${parentInfo}${posInfo}`);
    });
  }
}

/**
 * 打印 Undo/Redo 栈信息（用于调试）
 */
export function printUndoStacks(undoManager: any, label: string): void {
  console.log(`\n📚 === ${label} ===`);
  const undoStack = undoManager['undoStack'] || [];
  const redoStack = undoManager['redoStack'] || [];
  
  console.log(`Undo Stack (${undoStack.length}):`);
  undoStack.forEach((cmd: any, i: number) => {
    console.log(`  ${i + 1}. ${cmd.description || cmd.constructor.name}`);
  });
  
  console.log(`Redo Stack (${redoStack.length}):`);
  redoStack.forEach((cmd: any, i: number) => {
    console.log(`  ${i + 1}. ${cmd.description || cmd.constructor.name}`);
  });
}

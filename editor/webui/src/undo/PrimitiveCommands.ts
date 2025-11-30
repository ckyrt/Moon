/**
 * Moon Engine - Primitive Commands
 * 处理创建和删除图元（Cube, Sphere 等）的命令
 */

import type { Command } from './Command';
import { engine } from '@/utils/engine-bridge';
import { useEditorStore } from '@/store/editorStore';

/**
 * CreatePrimitiveCommand - 创建图元（Cube, Sphere, Cylinder 等）
 * 
 * 设计原则：
 * - 首次 Execute：创建节点并序列化
 * - Redo：反序列化恢复原 ID
 * - Undo：删除节点
 */
export class CreatePrimitiveCommand implements Command {
  private primitiveType: string;
  private createdNodeId?: number;
  private serializedNodeData?: string;  // 🎯 保存完整快照
  description: string;

  constructor(primitiveType: string) {
    this.primitiveType = primitiveType;
    this.description = `Create ${primitiveType}`;
  }

  async execute(): Promise<void> {
    if (this.serializedNodeData) {
      // 🎯 Redo：反序列化快照（恢复原始 ID）
      console.log('[CreatePrimitiveCommand] Redo: Deserializing node');
      await engine.deserializeNode(this.serializedNodeData);
    } else {
      // 🎯 首次 Execute：创建新节点
      console.log('[CreatePrimitiveCommand] Execute: Creating new', this.primitiveType);
      await engine.createPrimitive(this.primitiveType);

      // 刷新场景（获取新节点的 ID）
      const scene = await engine.getScene();
      
      // 查找新创建的节点（最新的节点）
      const allNodeIds = Object.keys(scene.allNodes).map(id => parseInt(id));
      if (allNodeIds.length > 0) {
        this.createdNodeId = Math.max(...allNodeIds);
        
        // 🎯 立即序列化快照（保存 ID 和完整状态）
        this.serializedNodeData = await engine.serializeNode(this.createdNodeId);
        console.log('[CreatePrimitiveCommand] Snapshot saved', { nodeId: this.createdNodeId });
      }
    }

    // 更新场景
    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }

  async undo(): Promise<void> {
    if (!this.createdNodeId) {
      console.warn('[CreatePrimitiveCommand] Cannot undo: createdNodeId is unknown');
      return;
    }

    console.log('[CreatePrimitiveCommand] Undo: Deleting node', this.createdNodeId);
    // 删除创建的节点
    await engine.deleteNode(this.createdNodeId);

    // 刷新场景
    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }
}

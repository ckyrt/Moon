/**
 * Moon Engine - Node Commands
 * 处理节点创建、删除、重命名等操作
 * 
 * 设计原则（参考 Unity/Unreal）：
 * 1. 命令创建时捕获所有必要状态
 * 2. Execute 和 Redo 行为完全一致（幂等）
 * 3. 使用完整快照而非增量修改
 * 4. 命令之间完全独立，无状态依赖
 */

import type { Command } from './Command';
import { engine } from '@/utils/engine-bridge';
import { useEditorStore } from '@/store/editorStore';
import { logger } from '@/utils/logger';

/**
 * CreateNodeCommand - 创建新节点
 * 
 * 快照内容：
 * - serializedNodeData: 创建后的完整节点数据（包含 ID）
 * 
 * 关键：
 * - 首次 Execute：创建新节点并保存快照
 * - Redo：反序列化快照（恢复原始 ID）
 * - Undo：删除节点
 */
export class CreateNodeCommand implements Command {
  private nodeType: string;
  private parentId?: number;
  private serializedNodeData?: string;  // 🎯 保存创建后的完整快照
  private createdNodeId?: number;
  description: string;

  constructor(nodeType: string, parentId?: number) {
    this.nodeType = nodeType;
    this.parentId = parentId;
    this.description = `Create ${nodeType} Node`;
  }

  /**
   * 获取创建的节点 ID（用于测试）
   */
  getCreatedNodeId(): number | undefined {
    return this.createdNodeId;
  }

  async execute(): Promise<void> {
    console.log('[CreateNodeCommand] execute() called', { hasSnapshot: !!this.serializedNodeData });
    logger.info('CreateNodeCommand', `Execute called`, { hasSnapshot: !!this.serializedNodeData });
    
    if (this.serializedNodeData) {
      // 🎯 Redo：反序列化快照（恢复原始 ID）
      logger.info('CreateNodeCommand', `Redo: Deserializing node`, { nodeType: this.nodeType, snapshotLength: this.serializedNodeData.length });
      
      try {
        await engine.deserializeNode(this.serializedNodeData);
        logger.info('CreateNodeCommand', `DeserializeNode succeeded`);
      } catch (error) {
        logger.error('CreateNodeCommand', `DeserializeNode failed!`, error);
        throw error;
      }
    } else {
      // 🎯 首次 Execute：创建新节点
      logger.info('CreateNodeCommand', `Execute: Creating new ${this.nodeType}`, { parentId: this.parentId });
      
      // 获取创建前的所有节点 ID
      const sceneBefore = await engine.getScene();
      const nodeIdsBefore = new Set(Object.keys(sceneBefore.allNodes).map(Number));
      
      await engine.createNode(this.nodeType, this.parentId);

      const sceneAfter = await engine.getScene();
      
      // 🎯 找到新创建的节点（ID 不在创建前的列表中）
      const allNodesAfter = Object.values(sceneAfter.allNodes);
      const newNode = allNodesAfter.find(n => !nodeIdsBefore.has(n.id));
      
      if (newNode) {
        this.createdNodeId = newNode.id;
        
        // 🎯 立即序列化快照（保存 ID）
        this.serializedNodeData = await engine.serializeNode(newNode.id);
        logger.info('CreateNodeCommand', `Snapshot saved`, { 
          createdNodeId: this.createdNodeId, 
          nodeName: newNode.name,
          snapshotLength: this.serializedNodeData.length 
        });
      } else {
        logger.error('CreateNodeCommand', `Failed to find newly created node!`);
      }
    }

    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }

  async undo(): Promise<void> {
    if (!this.createdNodeId) {
      console.warn('[CreateNodeCommand] Cannot undo: createdNodeId is unknown');
      return;
    }

    logger.info('CreateNodeCommand', `Undo: Deleting node`, { nodeId: this.createdNodeId });
    await engine.deleteNode(this.createdNodeId);

    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }
}

/**
 * DeleteNodeCommand - 删除节点
 * 
 * 快照内容：
 * - serializedNodeData: 删除前的完整节点数据（包含子树）
 * 
 * 关键：在构造时就序列化，不在 execute 时
 */
export class DeleteNodeCommand implements Command {
  private nodeId: number;
  private serializedNodeData: string;  // 🎯 构造时就确定
  description: string;

  private constructor(nodeId: number, serializedData: string) {
    this.nodeId = nodeId;
    this.serializedNodeData = serializedData;
    this.description = `Delete Node ${nodeId}`;
  }

  /**
   * 🎯 静态工厂方法：创建前先序列化
   */
  static async create(nodeId: number): Promise<DeleteNodeCommand> {
    const serializedData = await engine.serializeNode(nodeId);
    return new DeleteNodeCommand(nodeId, serializedData);
  }

  async execute(): Promise<void> {
    // 删除节点（包括子树）
    logger.info('DeleteNodeCommand', `Execute: Deleting node`, { nodeId: this.nodeId });
    await engine.deleteNode(this.nodeId);

    const updatedScene = await engine.getScene();
    useEditorStore.getState().updateScene(updatedScene);

    // 清除选中状态
    if (useEditorStore.getState().selectedNodeId === this.nodeId) {
      useEditorStore.getState().setSelectedNode(null);
    }
  }

  async undo(): Promise<void> {
    // 🎯 恢复完整快照（包含子树）
    logger.info('DeleteNodeCommand', `Undo: Restoring node`, { nodeId: this.nodeId });
    await engine.deserializeNode(this.serializedNodeData);
    
    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }
}

/**
 * RenameNodeCommand - 重命名节点
 * 
 * 快照内容：
 * - oldName: 旧名称
 * - newName: 新名称
 */
export class RenameNodeCommand implements Command {
  private nodeId: number;
  private oldName: string;
  private newName: string;
  description: string;

  constructor(nodeId: number, oldName: string, newName: string) {
    this.nodeId = nodeId;
    this.oldName = oldName;
    this.newName = newName;
    this.description = `Rename Node ${nodeId}`;
  }

  async execute(): Promise<void> {
    await engine.renameNode(this.nodeId, this.newName);

    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }

  async undo(): Promise<void> {
    await engine.renameNode(this.nodeId, this.oldName);

    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }
}

/**
 * SetNodeActiveCommand - 设置节点激活状态
 * 
 * 快照内容：
 * - oldActive: 旧状态
 * - newActive: 新状态
 */
export class SetNodeActiveCommand implements Command {
  private nodeId: number;
  private oldActive: boolean;
  private newActive: boolean;
  description: string;

  constructor(nodeId: number, oldActive: boolean, newActive: boolean) {
    this.nodeId = nodeId;
    this.oldActive = oldActive;
    this.newActive = newActive;
    this.description = `Set Node ${nodeId} Active: ${newActive}`;
  }

  async execute(): Promise<void> {
    await engine.setNodeActive(this.nodeId, this.newActive);

    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }

  async undo(): Promise<void> {
    await engine.setNodeActive(this.nodeId, this.oldActive);

    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }
}

/**
 * SetParentCommand - 设置节点父级
 * 
 * 快照内容：
 * - oldParentId: 旧父节点 ID
 * - newParentId: 新父节点 ID
 */
export class SetParentCommand implements Command {
  private nodeId: number;
  private oldParentId: number | null;
  private newParentId: number | null;
  description: string;

  constructor(nodeId: number, oldParentId: number | null, newParentId: number | null) {
    this.nodeId = nodeId;
    this.oldParentId = oldParentId;
    this.newParentId = newParentId;
    this.description = `Set Parent of Node ${nodeId}`;
  }

  async execute(): Promise<void> {
    logger.info('SetParentCommand', `Execute: Setting parent`, { nodeId: this.nodeId, oldParent: this.oldParentId, newParent: this.newParentId });
    await engine.setNodeParent(this.nodeId, this.newParentId);

    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }

  async undo(): Promise<void> {
    logger.info('SetParentCommand', `Undo: Reverting parent`, { nodeId: this.nodeId, oldParent: this.oldParentId, newParent: this.newParentId });
    await engine.setNodeParent(this.nodeId, this.oldParentId);

    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }
}

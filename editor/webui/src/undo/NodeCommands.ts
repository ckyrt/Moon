/**
 * Node Commands - Create/Delete/Rename/SetParent/SetActive operations
 * 
 * Design: Snapshot state at construction, execute() = redo()
 */

import type { Command } from './Command';
import { engine } from '@/utils/engine-bridge';
import { useEditorStore } from '@/store/editorStore';
import { logger } from '@/utils/logger';

/**
 * CreateNodeCommand - Create new node
 * 
 * Snapshot after first execute (node ID unknown until creation)
 */
export class CreateNodeCommand implements Command {
  private serializedNodeData?: string;
  private createdNodeId?: number;
  description: string;

  constructor(
    private nodeType: string,
    private parentId?: number
  ) {
    this.description = `Create ${nodeType} Node`;
  }

  getCreatedNodeId(): number | undefined {
    return this.createdNodeId;
  }

  async execute(): Promise<void> {
    if (this.serializedNodeData) {
      // Redo: Deserialize snapshot
      logger.info('CreateNodeCommand', `Redo: Deserializing node`, { nodeType: this.nodeType });
      await engine.deserializeNode(this.serializedNodeData);
    } else {
      // First execute: Create node and snapshot
      logger.info('CreateNodeCommand', `Execute: Creating ${this.nodeType}`, { parentId: this.parentId });
      
      const sceneBefore = await engine.getScene();
      const nodeIdsBefore = new Set(Object.keys(sceneBefore.allNodes).map(Number));
      
      await engine.createNode(this.nodeType, this.parentId);

      const sceneAfter = await engine.getScene();
      const allNodesAfter = Object.values(sceneAfter.allNodes);
      const newNode = allNodesAfter.find(n => !nodeIdsBefore.has(n.id));
      
      if (newNode) {
        this.createdNodeId = newNode.id;
        this.serializedNodeData = await engine.serializeNode(newNode.id);
        logger.info('CreateNodeCommand', `Snapshot saved`, { 
          nodeId: this.createdNodeId, 
          name: newNode.name
        });
      } else {
        logger.error('CreateNodeCommand', `Failed to find newly created node`);
      }
    }

    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }

  async undo(): Promise<void> {
    if (!this.createdNodeId) return;

    logger.info('CreateNodeCommand', `Undo: Deleting node`, { nodeId: this.createdNodeId });
    await engine.deleteNode(this.createdNodeId);

    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }
}

/**
 * DeleteNodeCommand - Delete node
 * 
 * Snapshot before deletion (static factory pattern)
 */
export class DeleteNodeCommand implements Command {
  description: string;

  private constructor(
    private nodeId: number,
    private serializedNodeData: string
  ) {
    this.description = `Delete Node ${nodeId}`;
  }

  static async create(nodeId: number): Promise<DeleteNodeCommand> {
    const serializedData = await engine.serializeNode(nodeId);
    return new DeleteNodeCommand(nodeId, serializedData);
  }

  async execute(): Promise<void> {
    logger.info('DeleteNodeCommand', `Execute: Deleting node`, { nodeId: this.nodeId });
    await engine.deleteNode(this.nodeId);

    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);

    if (useEditorStore.getState().selectedNodeId === this.nodeId) {
      useEditorStore.getState().setSelectedNode(null);
    }
  }

  async undo(): Promise<void> {
    logger.info('DeleteNodeCommand', `Undo: Restoring node`, { nodeId: this.nodeId });
    await engine.deserializeNode(this.serializedNodeData);
    
    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }
}

/**
 * RenameNodeCommand - Rename node
 */
export class RenameNodeCommand implements Command {
  description: string;

  constructor(
    private nodeId: number,
    private oldName: string,
    private newName: string
  ) {
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
 * SetNodeActiveCommand - Set node active state
 */
export class SetNodeActiveCommand implements Command {
  description: string;

  constructor(
    private nodeId: number,
    private oldActive: boolean,
    private newActive: boolean
  ) {
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
 * SetParentCommand - Change node parent
 */
export class SetParentCommand implements Command {
  description: string;

  constructor(
    private nodeId: number,
    private oldParentId: number | null,
    private newParentId: number | null
  ) {
    this.description = `Set Parent of Node ${nodeId}`;
  }

  async execute(): Promise<void> {
    logger.info('SetParentCommand', `Execute: Setting parent`, { 
      nodeId: this.nodeId, 
      oldParent: this.oldParentId, 
      newParent: this.newParentId 
    });
    await engine.setNodeParent(this.nodeId, this.newParentId);
    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }

  async undo(): Promise<void> {
    logger.info('SetParentCommand', `Undo: Reverting parent`, { 
      nodeId: this.nodeId, 
      oldParent: this.oldParentId, 
      newParent: this.newParentId 
    });
    await engine.setNodeParent(this.nodeId, this.oldParentId);
    const scene = await engine.getScene();
    useEditorStore.getState().updateScene(scene);
  }
}

/**
 * Moon Editor - Hierarchy Panel Component
 */

import React, { useEffect, useState } from 'react';
import { useEditorStore } from '@/store/editorStore';
import { engine } from '@/utils/engine-bridge';
import { getUndoManager, DeleteNodeCommand, CreateNodeCommand, SetParentCommand } from '@/undo';
import type { SceneNode } from '@/types/engine';
import { ContextMenu, ContextMenuItem } from './ContextMenu';
import styles from './Hierarchy.module.css';

export const Hierarchy: React.FC = () => {
  const { scene, selectedNodeId, setSelectedNode, updateScene } = useEditorStore();
  const [contextMenu, setContextMenu] = useState<{ x: number; y: number; nodeId: number } | null>(null);
  const [expandedNodes, setExpandedNodes] = useState<Set<number>>(new Set());
  const [draggedNodeId, setDraggedNodeId] = useState<number | null>(null);
  const [dropTargetId, setDropTargetId] = useState<number | null>(null);
  const [isDragging, setIsDragging] = useState(false);

  // Load scene data from engine on mount
  useEffect(() => {
    const loadScene = async () => {
      try {
        console.log('[Hierarchy] Loading scene from engine...');
        const sceneData = await engine.getScene();
        console.log('[Hierarchy] Loaded scene from engine:', sceneData);
        updateScene(sceneData);
        
        // Expand all nodes by default
        const allNodeIds = Object.keys(sceneData.allNodes).map(id => parseInt(id));
        setExpandedNodes(new Set(allNodeIds));
      } catch (error) {
        console.error('[Hierarchy] Failed to load scene:', error);
      }
    };

    loadScene();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const handleSelect = (nodeId: number) => {
    setSelectedNode(nodeId);
    engine.selectNode(nodeId);
  };

  const handleContextMenu = (event: React.MouseEvent, nodeId: number) => {
    event.preventDefault();
    event.stopPropagation();
    setContextMenu({ x: event.clientX, y: event.clientY, nodeId });
  };

  const handleDeleteNode = async (nodeId: number) => {
    try {
      // 🎯 使用静态工厂方法创建命令（会预先序列化）
      const command = await DeleteNodeCommand.create(nodeId);
      const undoManager = getUndoManager();
      
      // 执行命令并推入 undo 栈
      await command.execute();
      undoManager.pushExecutedCommand(command);
      
      console.log('[Hierarchy] Node deleted with Undo support:', nodeId);
    } catch (error) {
      console.error('[Hierarchy] Failed to delete node:', error);
    }
  };

  const handleAddChild = async (parentId: number) => {
    try {
      // 使用 Undo 系统创建节点
      const command = new CreateNodeCommand('New Node', parentId);
      const undoManager = getUndoManager();
      
      // 手动执行并推入栈
      await command.execute();
      undoManager.pushExecutedCommand(command);
      
      // Expand parent node to show new child
      setExpandedNodes(prev => new Set(prev).add(parentId));
      
      console.log('[Hierarchy] Node created with Undo support');
    } catch (error) {
      console.error('[Hierarchy] Failed to create child node:', error);
    }
  };

  const toggleExpand = (nodeId: number) => {
    setExpandedNodes(prev => {
      const next = new Set(prev);
      if (next.has(nodeId)) {
        next.delete(nodeId);
      } else {
        next.add(nodeId);
      }
      return next;
    });
  };

  // ✅ 使用鼠标事件模拟拖放（CEF OSR不支持HTML5 drag&drop）
  const handleMouseDown = (event: React.MouseEvent, nodeId: number) => {
    if (event.button !== 0) return; // 只处理左键
    event.stopPropagation();
    setDraggedNodeId(nodeId);
    setIsDragging(true);
    console.log('[Hierarchy] Mouse down on node:', nodeId);
  };

  const handleMouseEnter = (nodeId: number) => {
    if (!isDragging || draggedNodeId === null || draggedNodeId === nodeId) {
      return;
    }
    
    // Check if dropping on descendant
    const isDescendant = (parentId: number, childId: number): boolean => {
      const node = scene.allNodes[childId];
      if (!node) return false;
      if (node.parentId === parentId) return true;
      if (node.parentId === null) return false;
      return isDescendant(parentId, node.parentId);
    };
    
    if (!isDescendant(draggedNodeId, nodeId)) {
      setDropTargetId(nodeId);
      console.log('[Hierarchy] Mouse enter potential drop target:', nodeId);
    }
  };

  const handleMouseLeave = () => {
    if (isDragging) {
      setDropTargetId(null);
    }
  };

  const handleMouseUp = async (event: React.MouseEvent, targetNodeId: number) => {
    if (!isDragging || draggedNodeId === null) {
      setIsDragging(false);
      return;
    }
    
    event.stopPropagation();
    console.log('[Hierarchy] Mouse up:', { draggedNodeId, targetNodeId });
    
    if (draggedNodeId === targetNodeId) {
      setDraggedNodeId(null);
      setDropTargetId(null);
      setIsDragging(false);
      return;
    }
    
    try {
      // 获取旧的父级 ID
      const draggedNode = scene.allNodes[draggedNodeId];
      const oldParentId = draggedNode?.parentId ?? null;
      
      // Expand target node BEFORE executing command
      setExpandedNodes(prev => new Set(prev).add(targetNodeId));
      
      // 使用 Undo 系统改变父级
      const command = new SetParentCommand(draggedNodeId, oldParentId, targetNodeId);
      const undoManager = getUndoManager();
      
      // 手动执行并推入栈
      await command.execute();
      undoManager.pushExecutedCommand(command);
      
      console.log('[Hierarchy] Parent changed with Undo support');
    } catch (error) {
      console.error('[Hierarchy] Failed to change parent:', error);
    }
    
    setDraggedNodeId(null);
    setDropTargetId(null);
    setIsDragging(false);
  };

  // 全局鼠标抬起事件处理
  React.useEffect(() => {
    const handleGlobalMouseUp = () => {
      if (isDragging) {
        console.log('[Hierarchy] Global mouse up - cancel drag');
        setIsDragging(false);
        setDraggedNodeId(null);
        setDropTargetId(null);
      }
    };
    
    window.addEventListener('mouseup', handleGlobalMouseUp);
    return () => window.removeEventListener('mouseup', handleGlobalMouseUp);
  }, [isDragging]);

  const getContextMenuItems = (nodeId: number): ContextMenuItem[] => {
    return [
      {
        label: 'Add Child',
        onClick: () => handleAddChild(nodeId),
      },
      {
        label: 'Delete',
        onClick: () => handleDeleteNode(nodeId),
      },
    ];
  };

  const renderNode = (node: SceneNode, depth = 0) => {
    const isSelected = node.id === selectedNodeId;
    const hasChildren = node.children.length > 0;
    const isExpanded = expandedNodes.has(node.id);
    const isDraggedOver = dropTargetId === node.id;
    const isDraggingThis = draggedNodeId === node.id;

    return (
      <div key={node.id} className={styles.nodeContainer}>
        <div
          className={`${styles.node} ${isSelected ? styles.selected : ''} ${isDraggedOver ? styles.dragOver : ''} ${isDraggingThis ? styles.dragging : ''}`}
          style={{ paddingLeft: `${depth * 16 + 8}px`, cursor: isDraggingThis ? 'grabbing' : 'pointer' }}
          onClick={() => handleSelect(node.id)}
          onContextMenu={(e) => handleContextMenu(e, node.id)}
          onMouseDown={(e) => handleMouseDown(e, node.id)}
          onMouseEnter={() => handleMouseEnter(node.id)}
          onMouseLeave={handleMouseLeave}
          onMouseUp={(e) => handleMouseUp(e, node.id)}
        >
          {hasChildren ? (
            <span 
              className={`${styles.arrow} ${isExpanded ? styles.expanded : ''}`}
              onClick={(e) => {
                e.stopPropagation();
                toggleExpand(node.id);
              }}
            >
              ▸
            </span>
          ) : (
            <span className={styles.arrow}></span>
          )}
          <span className={styles.icon}>📦</span>
          <span className={styles.name}>{node.name}</span>
        </div>
        {hasChildren && isExpanded && (
          <div className={styles.children}>
            {node.children.map((childId) => {
              const child = scene.allNodes[childId];
              return child ? renderNode(child, depth + 1) : null;
            })}
          </div>
        )}
      </div>
    );
  };

  return (
    <div className={styles.hierarchy}>
      <div className={styles.header}>
        <span className={styles.title}>Hierarchy</span>
        <button className={styles.button} title="Create new node">
          +
        </button>
      </div>
      <div className={styles.content}>
        {!scene || !scene.rootNodes || scene.rootNodes.length === 0 ? (
          <div className={styles.empty}>No objects in scene</div>
        ) : (
          scene.rootNodes.map((nodeId) => {
            const node = scene.allNodes[nodeId];
            return node ? renderNode(node) : null;
          })
        )}
      </div>
      {contextMenu && (
        <ContextMenu
          position={{ x: contextMenu.x, y: contextMenu.y }}
          items={getContextMenuItems(contextMenu.nodeId)}
          onClose={() => setContextMenu(null)}
        />
      )}
    </div>
  );
};

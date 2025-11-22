/**
 * Moon Editor - Hierarchy Panel Component
 */

import React, { useEffect, useState } from 'react';
import { useEditorStore } from '@/store/editorStore';
import { engine } from '@/utils/engine-bridge';
import type { SceneNode } from '@/types/engine';
import { ContextMenu, ContextMenuItem } from './ContextMenu';
import styles from './Hierarchy.module.css';

export const Hierarchy: React.FC = () => {
  const { scene, selectedNodeId, setSelectedNode, updateScene } = useEditorStore();
  const [contextMenu, setContextMenu] = useState<{ x: number; y: number; nodeId: number } | null>(null);
  const [expandedNodes, setExpandedNodes] = useState<Set<number>>(new Set());
  const [draggedNodeId, setDraggedNodeId] = useState<number | null>(null);
  const [dropTargetId, setDropTargetId] = useState<number | null>(null);

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
      await engine.deleteNode(nodeId);
      // Reload scene
      const sceneData = await engine.getScene();
      updateScene(sceneData);
      // Clear selection if deleted node was selected
      if (selectedNodeId === nodeId) {
        setSelectedNode(null);
      }
    } catch (error) {
      console.error('[Hierarchy] Failed to delete node:', error);
    }
  };

  const handleAddChild = async (parentId: number) => {
    try {
      await engine.createNode('New Node', parentId);
      // Reload scene
      const sceneData = await engine.getScene();
      updateScene(sceneData);
      // Expand parent node to show new child
      setExpandedNodes(prev => new Set(prev).add(parentId));
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

  const handleDragStart = (event: React.DragEvent, nodeId: number) => {
    event.stopPropagation();
    setDraggedNodeId(nodeId);
    event.dataTransfer.effectAllowed = 'move';
  };

  const handleDragOver = (event: React.DragEvent, nodeId: number) => {
    event.preventDefault();
    event.stopPropagation();
    
    if (draggedNodeId === null || draggedNodeId === nodeId) {
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
      event.dataTransfer.dropEffect = 'move';
    }
  };

  const handleDragLeave = () => {
    setDropTargetId(null);
  };

  const handleDrop = async (event: React.DragEvent, targetNodeId: number) => {
    event.preventDefault();
    event.stopPropagation();
    
    if (draggedNodeId === null || draggedNodeId === targetNodeId) {
      setDraggedNodeId(null);
      setDropTargetId(null);
      return;
    }
    
    try {
      await engine.setNodeParent(draggedNodeId, targetNodeId);
      // Reload scene
      const sceneData = await engine.getScene();
      updateScene(sceneData);
      // Expand target node to show new child
      setExpandedNodes(prev => new Set(prev).add(targetNodeId));
    } catch (error) {
      console.error('[Hierarchy] Failed to change parent:', error);
    }
    
    setDraggedNodeId(null);
    setDropTargetId(null);
  };

  const handleDragEnd = () => {
    setDraggedNodeId(null);
    setDropTargetId(null);
  };

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
    const isDragging = draggedNodeId === node.id;

    return (
      <div key={node.id} className={styles.nodeContainer}>
        <div
          className={`${styles.node} ${isSelected ? styles.selected : ''} ${isDraggedOver ? styles.dragOver : ''} ${isDragging ? styles.dragging : ''}`}
          style={{ paddingLeft: `${depth * 16 + 8}px` }}
          draggable
          onClick={() => handleSelect(node.id)}
          onContextMenu={(e) => handleContextMenu(e, node.id)}
          onDragStart={(e) => handleDragStart(e, node.id)}
          onDragOver={(e) => handleDragOver(e, node.id)}
          onDragLeave={handleDragLeave}
          onDrop={(e) => handleDrop(e, node.id)}
          onDragEnd={handleDragEnd}
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

/**
 * Moon Engine - Editor Store (Zustand)
 * 全局编辑器状态管理
 */

import { create } from 'zustand';
import type { EditorState, SceneNode, Scene, Vector3, Transform } from '@/types/engine';
import { logger } from '@/utils/logger';

interface EditorStore extends EditorState {
  // Actions
  setSelectedNode: (nodeId: number | null) => void;
  setHoveredNode: (nodeId: number | null) => void;
  setGizmoMode: (mode: 'translate' | 'rotate' | 'scale') => void;
  updateScene: (scene: Scene) => void;
  updateNodeTransform: (nodeId: number, transform: Partial<Transform>) => void;
}

// 创建初始场景
const createInitialScene = (): Scene => ({
  name: 'Untitled Scene',
  rootNodes: [],
  allNodes: {},
});

export const useEditorStore = create<EditorStore>((set) => ({
  // Initial state
  selectedNodeId: null,
  hoveredNodeId: null,
  gizmoMode: 'translate',
  scene: createInitialScene(),

  // Actions
  setSelectedNode: (nodeId) => set({ selectedNodeId: nodeId }),
  
  setHoveredNode: (nodeId) => set({ hoveredNodeId: nodeId }),
  
  setGizmoMode: (mode) => set({ gizmoMode: mode }),
  
  updateScene: (scene) => {
    // 🎯 打印场景层级结构
    logger.info('EditorStore', '=== Scene Updated ===');
    logger.info('EditorStore', `Root nodes: ${scene.rootNodes?.join(', ') || 'none'}`);
    
    const buildHierarchy = (nodeId: number, indent: string) => {
      const node = scene.allNodes[nodeId];
      if (!node) {
        logger.warn('EditorStore', `Node ${nodeId} not found in allNodes!`);
        return;
      }
      logger.info('EditorStore', `${indent}${node.name}(${node.id}) parent=${node.parentId ?? 'null'}`);
      if (node.children && node.children.length > 0) {
        node.children.forEach((childId: number) => buildHierarchy(childId, indent + '  '));
      }
    };
    
    if (scene.rootNodes && scene.rootNodes.length > 0) {
      scene.rootNodes.forEach((id: number) => buildHierarchy(id, ''));
    } else {
      logger.info('EditorStore', 'Scene is empty');
    }
    
    set({ scene });
  },
  
  updateNodeTransform: (nodeId, transform) =>
    set((state) => {
      const node = state.scene.allNodes[nodeId];
      if (!node) return state;

      return {
        scene: {
          ...state.scene,
          allNodes: {
            ...state.scene.allNodes,
            [nodeId]: {
              ...node,
              transform: {
                ...node.transform,
                ...transform,
              },
            },
          },
        },
      };
    }),
}));

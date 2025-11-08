/**
 * Moon Engine - Editor Store (Zustand)
 * 全局编辑器状态管理
 */

import { create } from 'zustand';
import type { EditorState, SceneNode, Scene, Vector3, Transform } from '@/types/engine';

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
  
  updateScene: (scene) => set({ scene }),
  
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

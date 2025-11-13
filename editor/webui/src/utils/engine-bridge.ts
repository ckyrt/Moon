/**
 * Moon Engine - C++ Bridge
 * JavaScript ↔ C++ 通信接口封装
 */

import type { MoonEngineAPI, SceneNode, Scene, Transform, Vector3, Component } from '@/types/engine';

// 检查是否在真实的 CEF 环境中
const isRealEngine = typeof window !== 'undefined' && 'moonEngine' in window;

// Mock implementation for development (当 CEF 未连接时使用)
const createMockAPI = (): MoonEngineAPI => {
  let nodeIdCounter = 1;
  const mockScene: Scene = {
    name: 'Mock Scene',
    rootNodes: [1, 2],
    allNodes: {
      1: {
        id: 1,
        name: 'Camera',
        active: true,
        transform: {
          position: { x: 0, y: 2, z: -5 },
          rotation: { x: 0, y: 0, z: 0 },
          scale: { x: 1, y: 1, z: 1 },
        },
        parentId: null,
        children: [],
        components: [{ type: 'Camera', enabled: true, properties: {} }],
      },
      2: {
        id: 2,
        name: 'Cube',
        active: true,
        transform: {
          position: { x: 0, y: 0, z: 0 },
          rotation: { x: 0, y: 45, z: 0 },
          scale: { x: 1, y: 1, z: 1 },
        },
        parentId: null,
        children: [3],
        components: [{ type: 'MeshRenderer', enabled: true, properties: { mesh: 'cube' } }],
      },
      3: {
        id: 3,
        name: 'Child Sphere',
        active: true,
        transform: {
          position: { x: 2, y: 0, z: 0 },
          rotation: { x: 0, y: 0, z: 0 },
          scale: { x: 0.5, y: 0.5, z: 0.5 },
        },
        parentId: 2,
        children: [],
        components: [{ type: 'MeshRenderer', enabled: true, properties: { mesh: 'sphere' } }],
      },
    },
  };

  return {
    getScene: () => {
      console.log('[Mock Engine] getScene()');
      return mockScene;
    },

    createNode: (name: string, parentId?: number) => {
      console.log(`[Mock Engine] createNode("${name}", ${parentId})`);
      const newNode: SceneNode = {
        id: ++nodeIdCounter,
        name,
        active: true,
        transform: {
          position: { x: 0, y: 0, z: 0 },
          rotation: { x: 0, y: 0, z: 0 },
          scale: { x: 1, y: 1, z: 1 },
        },
        parentId: parentId ?? null,
        children: [],
        components: [],
      };
      mockScene.allNodes[newNode.id] = newNode;
      if (parentId) {
        mockScene.allNodes[parentId]?.children.push(newNode.id);
      } else {
        mockScene.rootNodes.push(newNode.id);
      }
      return newNode;
    },

    deleteNode: (nodeId: number) => {
      console.log(`[Mock Engine] deleteNode(${nodeId})`);
      const node = mockScene.allNodes[nodeId];
      if (!node) return;

      // Remove from parent or root
      if (node.parentId) {
        const parent = mockScene.allNodes[node.parentId];
        if (parent) {
          parent.children = parent.children.filter((id) => id !== nodeId);
        }
      } else {
        mockScene.rootNodes = mockScene.rootNodes.filter((id) => id !== nodeId);
      }

      // Delete node and children
      delete mockScene.allNodes[nodeId];
    },

    renameNode: (nodeId: number, newName: string) => {
      console.log(`[Mock Engine] renameNode(${nodeId}, "${newName}")`);
      const node = mockScene.allNodes[nodeId];
      if (node) node.name = newName;
    },

    setNodeActive: (nodeId: number, active: boolean) => {
      console.log(`[Mock Engine] setNodeActive(${nodeId}, ${active})`);
      const node = mockScene.allNodes[nodeId];
      if (node) node.active = active;
    },

    setNodeParent: (nodeId: number, parentId: number | null) => {
      console.log(`[Mock Engine] setNodeParent(${nodeId}, ${parentId})`);
      // Implementation omitted for brevity
    },

    setTransform: (nodeId: number, transform: Transform) => {
      console.log(`[Mock Engine] setTransform(${nodeId})`, transform);
      const node = mockScene.allNodes[nodeId];
      if (node) node.transform = transform;
    },

    setPosition: (nodeId: number, position: Vector3) => {
      console.log(`[Mock Engine] setPosition(${nodeId})`, position);
      const node = mockScene.allNodes[nodeId];
      if (node) node.transform.position = position;
    },

    setRotation: (nodeId: number, rotation: Vector3) => {
      console.log(`[Mock Engine] setRotation(${nodeId})`, rotation);
      const node = mockScene.allNodes[nodeId];
      if (node) node.transform.rotation = rotation;
    },

    setScale: (nodeId: number, scale: Vector3) => {
      console.log(`[Mock Engine] setScale(${nodeId})`, scale);
      const node = mockScene.allNodes[nodeId];
      if (node) node.transform.scale = scale;
    },

    selectNode: (nodeId: number | null) => {
      console.log(`[Mock Engine] selectNode(${nodeId})`);
    },

    addComponent: (nodeId: number, componentType: string) => {
      console.log(`[Mock Engine] addComponent(${nodeId}, "${componentType}")`);
      return null;
    },

    removeComponent: (nodeId: number, componentType: string) => {
      console.log(`[Mock Engine] removeComponent(${nodeId}, "${componentType}")`);
    },

    setViewportBounds: (x: number, y: number, width: number, height: number) => {
      console.log(`[Mock Engine] setViewportBounds(${x}, ${y}, ${width}, ${height})`);
    },

    createCSGPrimitive: (type, params) => {
      console.log(`[Mock Engine] createCSGPrimitive("${type}")`, params);
      return mockScene.allNodes[1]!; // Placeholder
    },

    performCSGOperation: (nodeId1, nodeId2, operation) => {
      console.log(`[Mock Engine] performCSGOperation(${nodeId1}, ${nodeId2}, "${operation}")`);
      return mockScene.allNodes[1]!; // Placeholder
    },
  };
};

// ============================================================================
// Real Engine API Wrapper (使用 window.moonEngine)
// ============================================================================
const createRealAPI = (): MoonEngineAPI => {
  const realEngine = window.moonEngine!;

  return {
    getScene: async () => {
      console.log('[Real Engine] getScene()');
      try {
        const sceneData = await realEngine.getScene();
        console.log('[Real Engine] Scene data received:', sceneData);
        return sceneData;
      } catch (error) {
        console.error('[Real Engine] getScene() failed:', error);
        throw error;
      }
    },

    createNode: (name: string, parentId?: number) => {
      console.log(`[Real Engine] createNode("${name}", ${parentId})`);
      // TODO: Implement in C++
      return {
        id: 0,
        name,
        active: true,
        transform: {
          position: { x: 0, y: 0, z: 0 },
          rotation: { x: 0, y: 0, z: 0 },
          scale: { x: 1, y: 1, z: 1 },
        },
        parentId: parentId ?? null,
        children: [],
        components: [],
      };
    },

    deleteNode: (nodeId: number) => {
      console.log(`[Real Engine] deleteNode(${nodeId})`);
      // TODO: Implement in C++
    },

    renameNode: (nodeId: number, newName: string) => {
      console.log(`[Real Engine] renameNode(${nodeId}, "${newName}")`);
      // TODO: Implement in C++
    },

    setNodeActive: (nodeId: number, active: boolean) => {
      console.log(`[Real Engine] setNodeActive(${nodeId}, ${active})`);
      // TODO: Implement in C++
    },

    setNodeParent: (nodeId: number, parentId: number | null) => {
      console.log(`[Real Engine] setNodeParent(${nodeId}, ${parentId})`);
      // TODO: Implement in C++
    },

    setTransform: (nodeId: number, transform: Transform) => {
      console.log(`[Real Engine] setTransform(${nodeId})`, transform);
      realEngine.setPosition(nodeId, transform.position);
      realEngine.setRotation(nodeId, transform.rotation);
      realEngine.setScale(nodeId, transform.scale);
    },

    setPosition: (nodeId: number, position: Vector3) => {
      console.log(`[Real Engine] setPosition(${nodeId})`, position);
      realEngine.setPosition(nodeId, position);
    },

    setRotation: (nodeId: number, rotation: Vector3) => {
      console.log(`[Real Engine] setRotation(${nodeId})`, rotation);
      realEngine.setRotation(nodeId, rotation);
    },

    setScale: (nodeId: number, scale: Vector3) => {
      console.log(`[Real Engine] setScale(${nodeId})`, scale);
      realEngine.setScale(nodeId, scale);
    },

    selectNode: (nodeId: number | null) => {
      console.log(`[Real Engine] selectNode(${nodeId})`);
      if (nodeId !== null) {
        realEngine.selectNode(nodeId);
      }
    },

    addComponent: (nodeId: number, componentType: string) => {
      console.log(`[Real Engine] addComponent(${nodeId}, "${componentType}")`);
      // TODO: Implement in C++
      return null;
    },

    removeComponent: (nodeId: number, componentType: string) => {
      console.log(`[Real Engine] removeComponent(${nodeId}, "${componentType}")`);
      // TODO: Implement in C++
    },

    setViewportBounds: (x: number, y: number, width: number, height: number) => {
      console.log(`[Real Engine] setViewportBounds(${x}, ${y}, ${width}, ${height})`);
      // Viewport bounds are handled separately via viewport-rect message
    },

    createCSGPrimitive: (type, params) => {
      console.log(`[Real Engine] createCSGPrimitive("${type}")`, params);
      // TODO: Implement in C++
      return {} as SceneNode;
    },

    performCSGOperation: (nodeId1, nodeId2, operation) => {
      console.log(`[Real Engine] performCSGOperation(${nodeId1}, ${nodeId2}, "${operation}")`);
      // TODO: Implement in C++
      return {} as SceneNode;
    },
  };
};

// Export the engine API (优先使用真实 API，回退到 Mock)
export const engine: MoonEngineAPI = isRealEngine ? createRealAPI() : createMockAPI();

// 辅助函数：检查是否连接到真实引擎
export const isConnectedToEngine = (): boolean => {
  return window.moonEngine !== undefined;
};

// ============================================================================
// Transform 更新回调（C++ 推送通知）
// ============================================================================
// 定义全局回调类型
declare global {
  interface Window {
    onTransformChanged?: (nodeId: number, position: Vector3) => void;
  }
}

// 注册 Transform 更新监听器
export const registerTransformCallback = (callback: (nodeId: number, position: Vector3) => void) => {
  window.onTransformChanged = (nodeId: number, position: Vector3) => {
    console.log(`[Engine Bridge] Transform changed: node=${nodeId}, pos=(${position.x}, ${position.y}, ${position.z})`);
    callback(nodeId, position);
  };
  console.log('[Engine Bridge] Transform callback registered');
};

// 注册选中状态更新监听器（C++ Pick 物体后推送）
declare global {
  interface Window {
    onNodeSelected?: (nodeId: number | null) => void;
  }
}

export const registerSelectionCallback = (callback: (nodeId: number | null) => void) => {
  window.onNodeSelected = (nodeId: number | null) => {
    console.log(`[Engine Bridge] Node selected: ${nodeId}`);
    callback(nodeId);
  };
  console.log('[Engine Bridge] Selection callback registered');
};

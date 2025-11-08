/**
 * Moon Engine - C++ Bridge
 * JavaScript ↔ C++ 通信接口封装
 */

import type { MoonEngineAPI, SceneNode, Scene, Transform, Vector3, Component } from '@/types/engine';

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

// Export the engine API
export const engine: MoonEngineAPI = window.moonEngine ?? createMockAPI();

// 辅助函数：检查是否连接到真实引擎
export const isConnectedToEngine = (): boolean => {
  return window.moonEngine !== undefined;
};

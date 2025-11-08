/**
 * Moon Engine - TypeScript Type Definitions
 * 引擎核心类型定义
 */

// ============ 场景节点 ============

export interface Vector3 {
  x: number;
  y: number;
  z: number;
}

export interface Transform {
  position: Vector3;
  rotation: Vector3; // Euler angles in degrees
  scale: Vector3;
}

export interface SceneNode {
  id: number;
  name: string;
  active: boolean;
  transform: Transform;
  parentId: number | null;
  children: number[]; // Child node IDs
  components: Component[];
}

export interface Component {
  type: string;
  enabled: boolean;
  properties: Record<string, unknown>;
}

export interface Scene {
  name: string;
  rootNodes: number[]; // Root node IDs
  allNodes: Record<number, SceneNode>;
}

// ============ 编辑器状态 ============

export interface EditorState {
  selectedNodeId: number | null;
  hoveredNodeId: number | null;
  gizmoMode: 'translate' | 'rotate' | 'scale';
  scene: Scene;
}

// ============ C++ Bridge API ============

export interface MoonEngineAPI {
  // Scene Management
  getScene(): Scene;
  createNode(name: string, parentId?: number): SceneNode;
  deleteNode(nodeId: number): void;
  renameNode(nodeId: number, newName: string): void;
  setNodeActive(nodeId: number, active: boolean): void;
  
  // Hierarchy
  setNodeParent(nodeId: number, parentId: number | null): void;
  
  // Transform
  setTransform(nodeId: number, transform: Transform): void;
  setPosition(nodeId: number, position: Vector3): void;
  setRotation(nodeId: number, rotation: Vector3): void;
  setScale(nodeId: number, scale: Vector3): void;
  
  // Selection
  selectNode(nodeId: number | null): void;
  
  // Component
  addComponent(nodeId: number, componentType: string): Component | null;
  removeComponent(nodeId: number, componentType: string): void;
  
  // CSG Operations (Phase 3)
  createCSGPrimitive(type: 'box' | 'sphere' | 'cylinder', params: Record<string, unknown>): SceneNode;
  performCSGOperation(nodeId1: number, nodeId2: number, operation: 'union' | 'subtract' | 'intersect'): SceneNode;
}

// ============ Window Extensions ============

declare global {
  interface Window {
    moonEngine?: MoonEngineAPI;
  }
}

export {};

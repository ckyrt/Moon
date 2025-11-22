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

export interface Quaternion {
  x: number;
  y: number;
  z: number;
  w: number;
}

export interface Transform {
  position: Vector3;
  rotation: Vector3; // Euler angles in degrees (UI only)
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
  [key: string]: any; // 允许组件有额外的属性
}

export interface MeshRendererComponent extends Component {
  type: 'MeshRenderer';
  visible: boolean;
  hasMesh: boolean;
}

export interface RigidBodyComponent extends Component {
  type: 'RigidBody';
  hasBody: boolean;
  mass: number;
  shapeType: 'Box' | 'Sphere' | 'Capsule' | 'Cylinder';
  size: [number, number, number];
  linearVelocity?: [number, number, number];
  angularVelocity?: [number, number, number];
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
  getScene(): Scene | Promise<Scene>;
  createNode(name: string, parentId?: number): Promise<void>;
  deleteNode(nodeId: number): Promise<void>;
  renameNode(nodeId: number, newName: string): void;
  setNodeActive(nodeId: number, active: boolean): void;
  
  // Hierarchy
  setNodeParent(nodeId: number, parentId: number | null): Promise<void>;
  
  // Transform
  setTransform(nodeId: number, transform: Transform): void;
  setPosition(nodeId: number, position: Vector3): void;
  setRotation(nodeId: number, rotation: Quaternion): void;
  setScale(nodeId: number, scale: Vector3): void;
  
  // Selection
  selectNode(nodeId: number | null): void;
  
  // Gizmo Mode
  setGizmoMode(mode: 'translate' | 'rotate' | 'scale'): void;
  
  // 🎯 Gizmo 坐标系模式（World/Local，Unity 风格）
  setGizmoCoordinateMode(mode: 'world' | 'local'): Promise<void>;
  
  // Component
  addComponent(nodeId: number, componentType: string): Component | null;
  removeComponent(nodeId: number, componentType: string): void;
  
  // Viewport Management
  setViewportBounds(x: number, y: number, width: number, height: number): void;
  
  // CSG Operations (Phase 3)
  createCSGPrimitive(type: 'box' | 'sphere' | 'cylinder', params: Record<string, unknown>): SceneNode;
  performCSGOperation(nodeId1: number, nodeId2: number, operation: 'union' | 'subtract' | 'intersect'): SceneNode;
  
  // Primitive Creation
  createPrimitive(type: string): Promise<void>;
}

// ============ Window Extensions ============

declare global {
  interface Window {
    moonEngine?: MoonEngineAPI;
    cefQuery?: (options: {
      request: string;
      onSuccess?: (response: string) => void;
      onFailure?: (errorCode: number, errorMessage: string) => void;
    }) => void;
  }
}

export {};

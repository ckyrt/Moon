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

export interface LightComponent extends Component {
  type: 'Light';
  lightType: 'Directional' | 'Point' | 'Spot';
  color: [number, number, number]; // RGB [0-1]
  intensity: number;
  range?: number; // For Point and Spot lights
  // Spot light specific
  innerConeAngle?: number;
  outerConeAngle?: number;
  // Attenuation (Point and Spot)
  attenuationConstant?: number;
  attenuationLinear?: number;
  attenuationQuadratic?: number;
}

export interface SkyboxComponent extends Component {
  type: 'Skybox';
  skyboxType: 'None' | 'EquirectangularHDR' | 'Cubemap' | 'Procedural';
  environmentMapPath: string; // HDR file path
  intensity: number; // Brightness multiplier
  rotation: number; // Y-axis rotation in degrees
  tint: [number, number, number]; // RGB tint [0-1]
  enableIBL: boolean; // Image-based lighting
}

export type MaterialPreset = 
  | 'None'
  | 'Concrete'
  | 'Fabric'
  | 'Metal'
  | 'Plastic'
  | 'Rock'
  | 'Wood'
  | 'Glass'
  | 'PolishedMetal';

export interface MaterialComponent extends Component {
  type: 'Material';
  preset: MaterialPreset; // Material preset
  metallic: number; // [0-1]
  roughness: number; // [0-1]
  baseColor: [number, number, number]; // RGB [0-1]
  albedoMap: string; // Color texture path
  normalMap: string; // Normal texture path
  aoMap: string; // Ambient Occlusion texture path
  roughnessMap: string; // Roughness texture path
  metalnessMap: string; // Metalness texture path
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

  // ========================================================================
  // 🎯 Component Properties API
  // ========================================================================
  
  // Material Component
  setMaterialMetallic(nodeId: number, metallic: number): Promise<void>;
  setMaterialRoughness(nodeId: number, roughness: number): Promise<void>;
  setMaterialBaseColor(nodeId: number, baseColor: Vector3): Promise<void>;
  setMaterialTexture(nodeId: number, textureType: string, path: string): Promise<void>;

  // ========================================================================
  // 🎯 Undo/Redo 专用 API（内部使用）
  // ========================================================================
  
  /**
   * 序列化节点（完整数据，包含所有 Components）
   * ⚠️ 内部 API：仅供 Undo/Redo 系统使用
   */
  serializeNode(nodeId: number): Promise<string>;
  
  /**
   * 反序列化节点（从完整数据重建节点和所有 Components）
   * ⚠️ 内部 API：仅供 Undo/Redo 系统使用
   */
  deserializeNode(serializedData: string): Promise<void>;
  
  /**
   * 批量设置 Transform（用于 Undo 快速恢复）
   * ⚠️ 内部 API：仅供 Undo/Redo 系统使用
   */
  setNodeTransform(nodeId: number, transform: Transform): Promise<void>;
  
  /**
   * 创建节点并指定 ID（用于 Undo 恢复被删除的节点）
   * ⚠️ 内部 API：仅供 Undo/Redo 系统使用
   * ⚠️ 已弃用：请使用 deserializeNode 替代（更完整）
   */
  createNodeWithId(
    nodeId: number,
    name: string,
    type: string,
    parentId: number | null,
    transform?: Transform
  ): Promise<void>;
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

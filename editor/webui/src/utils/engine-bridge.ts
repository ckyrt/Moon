/**
 * Moon Engine - C++ Bridge
 * 
 * JavaScript ↔ C++ 通信接口封装
 * 通过 CEF (Chromium Embedded Framework) 实现双向通信
 * 
 * 通信机制：
 * - JS → C++: window.moonEngine API (通过 cefQuery 发送 JSON 请求)
 * - C++ → JS: window.onXXXChanged 回调 (通过 ExecuteJavaScript 调用)
 */

import type { MoonEngineAPI, SceneNode, Transform, Vector3 } from '@/types/engine';

// ============================================================================
// Engine API 初始化
// ============================================================================

// 检查是否在真实的 CEF 环境中
const isRealEngine = typeof window !== 'undefined' && 'moonEngine' in window;

/**
 * 创建真实引擎 API 包装器
 * 为所有 API 调用添加日志记录
 */
const createRealAPI = (): MoonEngineAPI => {
  const realEngine = window.moonEngine!;

  // 辅助函数：包装同步引擎调用并添加日志
  const wrapEngineCall = <T extends any[], R>(
    methodName: string, 
    fn: (...args: T) => R
  ) => {
    return (...args: T): R => {
      console.log(`[Engine API] ${methodName}(${args.map(a => JSON.stringify(a)).join(', ')})`);
      return fn(...args);
    };
  };

  // 辅助函数：包装异步引擎调用
  const wrapAsyncEngineCall = <T extends any[], R>(
    methodName: string,
    fn: (...args: T) => Promise<R>
  ) => {
    return async (...args: T): Promise<R> => {
      console.log(`[Engine API] ${methodName}(${args.map(a => JSON.stringify(a)).join(', ')})`);
      try {
        const result = await fn(...args);
        console.log(`[Engine API] ${methodName} completed:`, result);
        return result;
      } catch (error) {
        console.error(`[Engine API] ${methodName} failed:`, error);
        throw error;
      }
    };
  };

  return {
    // ========== Scene Management ==========
    getScene: wrapAsyncEngineCall('getScene', async () => realEngine.getScene()),

    // ========== Node Management (未实现) ==========
    createNode: wrapEngineCall('createNode', (name: string, parentId?: number) => {
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
    }),

    deleteNode: wrapEngineCall('deleteNode', (_nodeId: number) => {
      // TODO: Implement in C++
    }),

    renameNode: wrapEngineCall('renameNode', (_nodeId: number, _newName: string) => {
      // TODO: Implement in C++
    }),

    setNodeActive: wrapEngineCall('setNodeActive', (_nodeId: number, _active: boolean) => {
      // TODO: Implement in C++
    }),

    setNodeParent: wrapEngineCall('setNodeParent', (_nodeId: number, _parentId: number | null) => {
      // TODO: Implement in C++
    }),

    // ========== Transform Operations ==========
    setTransform: wrapEngineCall('setTransform', (nodeId: number, transform: Transform) => {
      realEngine.setPosition(nodeId, transform.position);
      realEngine.setRotation(nodeId, transform.rotation);
      realEngine.setScale(nodeId, transform.scale);
    }),

    setPosition: wrapEngineCall('setPosition', (nodeId: number, position: Vector3) => {
      realEngine.setPosition(nodeId, position);
    }),

    setRotation: wrapEngineCall('setRotation', (nodeId: number, rotation: Vector3) => {
      realEngine.setRotation(nodeId, rotation);
    }),

    setScale: wrapEngineCall('setScale', (nodeId: number, scale: Vector3) => {
      realEngine.setScale(nodeId, scale);
    }),

    // ========== Selection & Gizmo ==========
    selectNode: wrapEngineCall('selectNode', (nodeId: number | null) => {
      if (nodeId !== null) {
        realEngine.selectNode(nodeId);
      }
    }),

    setGizmoMode: wrapEngineCall('setGizmoMode', (mode: 'translate' | 'rotate' | 'scale') => {
      realEngine.setGizmoMode(mode);
    }),

    // 🎯 设置 Gizmo 坐标系模式（world/local）
    setGizmoCoordinateMode: wrapAsyncEngineCall('setGizmoCoordinateMode', async (mode: 'world' | 'local') => {
      if (!window.cefQuery) {
        console.warn('[setGizmoCoordinateMode] cefQuery not available');
        return;
      }

      return new Promise<void>((resolve, reject) => {
        const request = JSON.stringify({
          command: 'setGizmoCoordinateMode',
          mode: mode
        });

        window.cefQuery({
          request: request,
          onSuccess: (response: string) => {
            console.log('[setGizmoCoordinateMode] Success:', response);
            resolve();
          },
          onFailure: (error_code: number, error_message: string) => {
            console.error('[setGizmoCoordinateMode] Failed:', error_message);
            reject(new Error(error_message));
          }
        });
      });
    }),

    // ========== Component Management (未实现) ==========
    addComponent: wrapEngineCall('addComponent', (_nodeId: number, _componentType: string) => {
      // TODO: Implement in C++
      return null;
    }),

    removeComponent: wrapEngineCall('removeComponent', (_nodeId: number, _componentType: string) => {
      // TODO: Implement in C++
    }),

    // ========== Viewport Management ==========
    setViewportBounds: wrapEngineCall('setViewportBounds', (_x: number, _y: number, _width: number, _height: number) => {
      // Viewport bounds are handled separately via viewport-rect message
    }),

    // ========== CSG Operations (未实现) ==========
    createCSGPrimitive: wrapEngineCall('createCSGPrimitive', (_type, _params) => {
      // TODO: Implement in C++
      return {} as SceneNode;
    }),

    performCSGOperation: wrapEngineCall('performCSGOperation', (_nodeId1, _nodeId2, _operation) => {
      // TODO: Implement in C++
      return {} as SceneNode;
    }),

    // ========== Primitive Creation ==========
    createPrimitive: wrapAsyncEngineCall('createPrimitive', async (type: string) => {
      if (!window.cefQuery) {
        console.warn('[createPrimitive] cefQuery not available');
        return;
      }

      return new Promise<void>((resolve, reject) => {
        const request = JSON.stringify({
          command: 'createNode',
          type
        });

        window.cefQuery!({
          request,
          onSuccess: (response: string) => {
            console.log(`[createPrimitive] Success: ${response}`);
            resolve();
          },
          onFailure: (errorCode: number, errorMessage: string) => {
            console.error(`[createPrimitive] Failed: ${errorCode} - ${errorMessage}`);
            reject(new Error(errorMessage));
          }
        });
      });
    }),
  };
};

// ============================================================================
// API 导出
// ============================================================================

/**
 * Moon Engine API 实例
 * 在 CEF 环境中自动连接到 C++，否则为空对象
 */
export const engine: MoonEngineAPI = isRealEngine ? createRealAPI() : ({} as MoonEngineAPI);

/**
 * 检查是否连接到真实引擎
 */
export const isConnectedToEngine = (): boolean => {
  return window.moonEngine !== undefined;
};

// ============================================================================
// C++ → JavaScript 回调注册
// ============================================================================

/**
 * 全局回调类型定义
 */
declare global {
  interface Window {
    onTransformChanged?: (nodeId: number, position: Vector3) => void;
    onRotationChanged?: (nodeId: number, rotation: Vector3) => void;
    onScaleChanged?: (nodeId: number, scale: Vector3) => void;
    onNodeSelected?: (nodeId: number | null) => void;
  }
}

/**
 * 注册 Position 更新监听器
 * C++ 在 Gizmo 拖动时调用 window.onTransformChanged
 */
export const registerTransformCallback = (callback: (nodeId: number, position: Vector3) => void) => {
  window.onTransformChanged = (nodeId: number, position: Vector3) => {
    console.log(`[C++ Callback] Position changed: node=${nodeId}, pos=(${position.x}, ${position.y}, ${position.z})`);
    callback(nodeId, position);
  };
  console.log('[Engine Bridge] Position callback registered');
};

/**
 * 注册 Rotation 更新监听器
 * C++ 在 Gizmo 旋转时调用 window.onRotationChanged
 */
export const registerRotationCallback = (callback: (nodeId: number, rotation: Vector3) => void) => {
  window.onRotationChanged = (nodeId: number, rotation: Vector3) => {
    console.log(`[C++ Callback] Rotation changed: node=${nodeId}, rot=(${rotation.x}, ${rotation.y}, ${rotation.z})`);
    callback(nodeId, rotation);
  };
  console.log('[Engine Bridge] Rotation callback registered');
};

/**
 * 注册 Scale 更新监听器
 * C++ 在 Gizmo 缩放时调用 window.onScaleChanged
 */
export const registerScaleCallback = (callback: (nodeId: number, scale: Vector3) => void) => {
  window.onScaleChanged = (nodeId: number, scale: Vector3) => {
    console.log(`[C++ Callback] Scale changed: node=${nodeId}, scale=(${scale.x}, ${scale.y}, ${scale.z})`);
    callback(nodeId, scale);
  };
  console.log('[Engine Bridge] Scale callback registered');
};

/**
 * 注册选中状态更新监听器
 * C++ 在 Pick 物体后调用 window.onNodeSelected
 */
export const registerSelectionCallback = (callback: (nodeId: number | null) => void) => {
  window.onNodeSelected = (nodeId: number | null) => {
    console.log(`[C++ Callback] Node selected: ${nodeId}`);
    callback(nodeId);
  };
  console.log('[Engine Bridge] Selection callback registered');
};

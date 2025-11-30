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

import type { MoonEngineAPI, SceneNode, Transform, Vector3, Quaternion } from '@/types/engine';
import { quaternionToEuler, eulerToQuaternion } from './math';

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
    getScene: wrapAsyncEngineCall('getScene', async () => {
      const scene = await realEngine.getScene();
      
      // Convert quaternions to Euler angles for UI display
      if (scene && scene.allNodes) {
        Object.values(scene.allNodes).forEach((node: any) => {
          if (node.transform && node.transform.rotation) {
            const rot = node.transform.rotation;
            // If rotation has 'w' component, it's a quaternion - convert to Euler
            if ('w' in rot) {
              node.transform.rotation = quaternionToEuler(rot as Quaternion);
            }
          }
        });
      }
      
      return scene;
    }),

    // ========== Node Management ==========
    createNode: wrapAsyncEngineCall('createNode', async (_name: string, parentId?: number) => {
      if (!window.cefQuery) {
        console.warn('[createNode] cefQuery not available');
        return;
      }

      return new Promise<void>((resolve, reject) => {
        const request = JSON.stringify({
          command: 'createNode',
          type: _name,
          parentId: parentId ?? null
        });

        window.cefQuery!({
          request,
          onSuccess: (_response: string) => {
            console.log('[createNode] Success');
            resolve();
          },
          onFailure: (_errorCode: number, errorMessage: string) => {
            console.error('[createNode] Failed:', errorMessage);
            reject(new Error(errorMessage));
          }
        });
      });
    }),

    deleteNode: wrapAsyncEngineCall('deleteNode', async (nodeId: number) => {
      if (!window.cefQuery) {
        console.warn('[deleteNode] cefQuery not available');
        return;
      }

      return new Promise<void>((resolve, reject) => {
        const request = JSON.stringify({
          command: 'deleteNode',
          nodeId
        });

        window.cefQuery!({
          request,
          onSuccess: (_response: string) => {
            console.log('[deleteNode] Success');
            resolve();
          },
          onFailure: (_errorCode: number, errorMessage: string) => {
            console.error('[deleteNode] Failed:', errorMessage);
            reject(new Error(errorMessage));
          }
        });
      });
    }),

    renameNode: wrapAsyncEngineCall('renameNode', async (nodeId: number, newName: string) => {
      if (!window.cefQuery) {
        console.warn('[renameNode] cefQuery not available');
        return;
      }

      return new Promise<void>((resolve, reject) => {
        const request = JSON.stringify({
          command: 'renameNode',
          nodeId: nodeId,
          newName: newName
        });

        window.cefQuery!({
          request,
          onSuccess: (_response: string) => {
            resolve();
          },
          onFailure: (_errorCode: number, errorMessage: string) => {
            console.error('[renameNode] Failed:', errorMessage);
            reject(new Error(errorMessage));
          }
        });
      });
    }),

    setNodeActive: wrapAsyncEngineCall('setNodeActive', async (nodeId: number, active: boolean) => {
      if (!window.cefQuery) {
        console.warn('[setNodeActive] cefQuery not available');
        return;
      }

      return new Promise<void>((resolve, reject) => {
        const request = JSON.stringify({
          command: 'setNodeActive',
          nodeId: nodeId,
          active: active
        });

        window.cefQuery!({
          request,
          onSuccess: (_response: string) => {
            resolve();
          },
          onFailure: (_errorCode: number, errorMessage: string) => {
            console.error('[setNodeActive] Failed:', errorMessage);
            reject(new Error(errorMessage));
          }
        });
      });
    }),

    setNodeParent: wrapAsyncEngineCall('setNodeParent', async (nodeId: number, parentId: number | null) => {
      if (!window.cefQuery) {
        console.warn('[setNodeParent] cefQuery not available');
        return;
      }

      return new Promise<void>((resolve, reject) => {
        const request = JSON.stringify({
          command: 'setNodeParent',
          nodeId,
          parentId
        });

        window.cefQuery!({
          request,
          onSuccess: (_response: string) => {
            console.log('[setNodeParent] Success');
            resolve();
          },
          onFailure: (_errorCode: number, errorMessage: string) => {
            console.error('[setNodeParent] Failed:', errorMessage);
            reject(new Error(errorMessage));
          }
        });
      });
    }),

    // ========== Transform Operations ==========
    setTransform: wrapEngineCall('setTransform', (nodeId: number, transform: Transform) => {
      realEngine.setPosition(nodeId, transform.position);
      // Convert Euler angles (UI) to Quaternion (engine)
      const quat = eulerToQuaternion(transform.rotation);
      realEngine.setRotation(nodeId, quat);
      realEngine.setScale(nodeId, transform.scale);
    }),

    setPosition: wrapEngineCall('setPosition', (nodeId: number, position: Vector3) => {
      realEngine.setPosition(nodeId, position);
    }),

    setRotation: wrapEngineCall('setRotation', (nodeId: number, rotation: Vector3) => {
      // Convert Euler angles (UI) to Quaternion (engine)
      const quat = eulerToQuaternion(rotation);
      realEngine.setRotation(nodeId, quat);
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

        window.cefQuery!({
          request: request,
          onSuccess: (response: string) => {
            console.log('[setGizmoCoordinateMode] Success:', response);
            resolve();
          },
          onFailure: (_errorCode: number, errorMessage: string) => {
            console.error('[setGizmoCoordinateMode] Failed:', errorMessage);
            reject(new Error(errorMessage));
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

    // ========================================================================
    // 🎯 Undo/Redo 专用 API（内部使用）
    // ========================================================================
    
    /**
     * 序列化节点（完整数据，用于 Delete Undo）
     * ⚠️ 内部 API：仅供 Undo/Redo 系统使用
     */
    serializeNode: wrapAsyncEngineCall('serializeNode', async (nodeId: number): Promise<string> => {
      if (!window.cefQuery) {
        console.warn('[serializeNode] cefQuery not available');
        return '{}';
      }

      return new Promise<string>((resolve, reject) => {
        const request = JSON.stringify({
          command: 'serializeNode',
          nodeId
        });

        window.cefQuery!({
          request,
          onSuccess: (response: string) => {
            const result = JSON.parse(response);
            if (result.success) {
              console.log('[serializeNode] Success');
              resolve(result.data);
            } else {
              reject(new Error('Serialization failed'));
            }
          },
          onFailure: (_errorCode: number, errorMessage: string) => {
            console.error('[serializeNode] Failed:', errorMessage);
            reject(new Error(errorMessage));
          }
        });
      });
    }),

    /**
     * 反序列化节点（从完整数据重建，用于 Delete Undo）
     * ⚠️ 内部 API：仅供 Undo/Redo 系统使用
     */
    deserializeNode: wrapAsyncEngineCall('deserializeNode', async (serializedData: string) => {
      if (!window.cefQuery) {
        console.warn('[deserializeNode] cefQuery not available');
        return;
      }

      return new Promise<void>((resolve, reject) => {
        const request = JSON.stringify({
          command: 'deserializeNode',
          data: serializedData
        });

        window.cefQuery!({
          request,
          onSuccess: (_response: string) => {
            console.log('[deserializeNode] Success');
            resolve();
          },
          onFailure: (_errorCode: number, errorMessage: string) => {
            console.error('[deserializeNode] Failed:', errorMessage);
            reject(new Error(errorMessage));
          }
        });
      });
    }),

    /**
     * 批量设置 Transform（用于 Undo 快速恢复）
     * ⚠️ 内部 API：仅供 Undo/Redo 系统使用
     */
    setNodeTransform: wrapAsyncEngineCall('setNodeTransform', async (nodeId: number, transform: Transform) => {
      if (!window.cefQuery) {
        console.warn('[setNodeTransform] cefQuery not available');
        return;
      }

      return new Promise<void>((resolve, reject) => {
        const request = JSON.stringify({
          command: 'setNodeTransform',
          nodeId,
          transform: {
            position: transform.position,
            rotation: eulerToQuaternion(transform.rotation), // 转换为四元数
            scale: transform.scale
          }
        });

        window.cefQuery!({
          request,
          onSuccess: (_response: string) => {
            console.log('[setNodeTransform] Success');
            resolve();
          },
          onFailure: (_errorCode: number, errorMessage: string) => {
            console.error('[setNodeTransform] Failed:', errorMessage);
            reject(new Error(errorMessage));
          }
        });
      });
    }),

    /**
     * 创建节点并指定 ID（用于 Undo 恢复被删除的节点）
     * ⚠️ 内部 API：仅供 Undo/Redo 系统使用
     */
    createNodeWithId: wrapAsyncEngineCall('createNodeWithId', async (
      nodeId: number,
      name: string,
      type: string,
      parentId: number | null,
      transform?: Transform
    ) => {
      if (!window.cefQuery) {
        console.warn('[createNodeWithId] cefQuery not available');
        return;
      }

      return new Promise<void>((resolve, reject) => {
        const request = JSON.stringify({
          command: 'createNodeWithId',
          nodeId,
          name,
          type,
          parentId,
          transform: transform ? {
            position: transform.position,
            rotation: eulerToQuaternion(transform.rotation),
            scale: transform.scale
          } : undefined
        });

        window.cefQuery!({
          request,
          onSuccess: (_response: string) => {
            console.log('[createNodeWithId] Success');
            resolve();
          },
          onFailure: (_errorCode: number, errorMessage: string) => {
            console.error('[createNodeWithId] Failed:', errorMessage);
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
    onRotationChanged?: (nodeId: number, rotation: Quaternion) => void;
    onScaleChanged?: (nodeId: number, scale: Vector3) => void;
    onNodeSelected?: (nodeId: number | null) => void;
    onGizmoStart?: (nodeId: number) => void;
    onGizmoEnd?: (nodeId: number, position: Vector3, rotation: Quaternion, scale: Vector3) => void;
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
 * C++ 在 Gizmo 旋转时调用 window.onRotationChanged（发送四元数）
 */
export const registerRotationCallback = (callback: (nodeId: number, rotation: Quaternion) => void) => {
  window.onRotationChanged = (nodeId: number, rotation: Quaternion) => {
    console.log(`[C++ Callback] Rotation changed: node=${nodeId}, quat=(${rotation.x}, ${rotation.y}, ${rotation.z}, ${rotation.w})`);
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

/**
 * 注册 Gizmo 开始拖拽监听器
 * C++ 在 Gizmo 开始拖拽时调用 window.onGizmoStart
 */
export const registerGizmoStartCallback = (callback: (nodeId: number) => void) => {
  window.onGizmoStart = (nodeId: number) => {
    console.log(`[C++ Callback] Gizmo drag started: node=${nodeId}`);
    callback(nodeId);
  };
  console.log('[Engine Bridge] Gizmo start callback registered');
};

/**
 * 注册 Gizmo 结束拖拽监听器
 * C++ 在 Gizmo 拖拽结束时调用 window.onGizmoEnd
 */
export const registerGizmoEndCallback = (
  callback: (nodeId: number, position: Vector3, rotation: Quaternion, scale: Vector3) => void
) => {
  window.onGizmoEnd = (nodeId: number, position: Vector3, rotation: Quaternion, scale: Vector3) => {
    console.log(`[C++ Callback] Gizmo drag ended: node=${nodeId}`);
    callback(nodeId, position, rotation, scale);
  };
  console.log('[Engine Bridge] Gizmo end callback registered');
};

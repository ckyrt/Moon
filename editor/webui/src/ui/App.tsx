/**
 * Moon Editor - Main Application Component
 * 
 * 职责：
 * 1. 初始化加载场景数据
 * 2. 注册 C++ → JavaScript 回调
 * 3. 渲染编辑器布局
 * 4. 注册 Undo/Redo 快捷键
 */

import React, { useEffect } from 'react';
import { EditorLayout } from '../components/EditorLayout';
import { useEditorStore } from '../store/editorStore';
import { 
  engine, 
  registerTransformCallback, 
  registerRotationCallback, 
  registerScaleCallback, 
  registerSelectionCallback,
  registerGizmoStartCallback,
  registerGizmoEndCallback,
} from '../utils/engine-bridge';
import { quaternionToEuler } from '../utils/math';
import { useUndoShortcuts, onGizmoStart, onGizmoEnd } from '@/undo';
import '../styles/global.css';

export const App: React.FC = () => {
  const updateScene = useEditorStore((state) => state.updateScene);
  const updateNodeTransform = useEditorStore((state) => state.updateNodeTransform);
  const setSelectedNode = useEditorStore((state) => state.setSelectedNode);

  // ========== Undo/Redo 快捷键 ==========
  useUndoShortcuts();

  // ========== 场景初始化 ==========
  
  useEffect(() => {
    const loadScene = async () => {
      try {
        console.log('[App] Loading scene from engine...');
        const scene = await engine.getScene();
        console.log('[App] Loaded scene:', scene);
        updateScene(scene);
      } catch (error) {
        console.error('[App] Failed to load scene:', error);
      }
    };
    
    loadScene();
  }, [updateScene]);

  // ========== C++ 回调注册 ==========
  
  // Position 更新（Gizmo 拖动）
  useEffect(() => {
    registerTransformCallback((nodeId, position) => {
      console.log(`[App] C++ → JS: Position changed (node=${nodeId})`, position);
      updateNodeTransform(nodeId, { position });
    });
  }, [updateNodeTransform]);

  // Rotation 更新（Gizmo 旋转，接收四元数并转换为欧拉角）
  useEffect(() => {
    registerRotationCallback((nodeId, quaternion) => {
      console.log(`[App] C++ → JS: Rotation changed (node=${nodeId})`, quaternion);
      // 将四元数转换为欧拉角（仅用于UI显示）
      const rotation = quaternionToEuler(quaternion);
      updateNodeTransform(nodeId, { rotation });
    });
  }, [updateNodeTransform]);

  // Scale 更新（Gizmo 缩放）
  useEffect(() => {
    registerScaleCallback((nodeId, scale) => {
      console.log(`[App] C++ → JS: Scale changed (node=${nodeId})`, scale);
      updateNodeTransform(nodeId, { scale });
    });
  }, [updateNodeTransform]);

  // Selection 更新（鼠标 Pick）
  useEffect(() => {
    registerSelectionCallback((nodeId) => {
      console.log(`[App] C++ → JS: Node selected (node=${nodeId})`);
      setSelectedNode(nodeId);
    });
  }, [setSelectedNode]);

  // ========== Gizmo Undo 集成 ==========

  // Gizmo 开始拖拽
  useEffect(() => {
    registerGizmoStartCallback((nodeId) => {
      console.log(`[App] C++ → JS: Gizmo drag started (node=${nodeId})`);
      onGizmoStart(nodeId);
    });
  }, []);

  // Gizmo 结束拖拽
  useEffect(() => {
    registerGizmoEndCallback((nodeId, position, quaternion, scale) => {
      console.log(`[App] C++ → JS: Gizmo drag ended (node=${nodeId})`);
      // 将四元数转换为欧拉角
      const rotation = quaternionToEuler(quaternion);
      onGizmoEnd(nodeId, position, rotation, scale, quaternion);
    });
  }, []);

  return <EditorLayout />;
};

/**
 * Moon Editor - Main App Component
 */

import React, { useEffect } from 'react';
import { EditorLayout } from '../components/EditorLayout';
import { useEditorStore } from '../store/editorStore';
import { engine, registerTransformCallback, registerSelectionCallback } from '../utils/engine-bridge';
import '../styles/global.css';

export const App: React.FC = () => {
  const updateScene = useEditorStore((state) => state.updateScene);
  const updateNodeTransform = useEditorStore((state) => state.updateNodeTransform);
  const setSelectedNode = useEditorStore((state) => state.setSelectedNode);

  // Load initial scene from engine
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

  // Register Transform update callback from C++
  useEffect(() => {
    registerTransformCallback((nodeId, position) => {
      console.log(`[App] Received transform update from C++: node=${nodeId}`, position);
      updateNodeTransform(nodeId, { position });
    });
  }, [updateNodeTransform]);

  // Register Selection update callback from C++ (Pick 操作)
  useEffect(() => {
    registerSelectionCallback((nodeId) => {
      console.log(`[App] Received selection update from C++: node=${nodeId}`);
      setSelectedNode(nodeId);
    });
  }, [setSelectedNode]);

  return <EditorLayout />;
};

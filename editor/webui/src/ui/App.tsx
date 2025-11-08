/**
 * Moon Editor - Main App Component
 */

import React, { useEffect } from 'react';
import { EditorLayout } from '../components/EditorLayout';
import { useEditorStore } from '../store/editorStore';
import { engine } from '../utils/engine-bridge';
import '../styles/global.css';

export const App: React.FC = () => {
  const updateScene = useEditorStore((state) => state.updateScene);

  // Load initial scene from engine
  useEffect(() => {
    const scene = engine.getScene();
    updateScene(scene);
    
    console.log('[Moon Editor] Initialized with scene:', scene);
  }, [updateScene]);

  return <EditorLayout />;
};

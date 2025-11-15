/**
 * Moon Editor - Toolbar Component
 */

import React from 'react';
import { useEditorStore } from '@/store/editorStore';
import { engine } from '@/utils/engine-bridge';
import styles from './Toolbar.module.css';

export const Toolbar: React.FC = () => {
  const { gizmoMode, setGizmoMode } = useEditorStore();

  const handleGizmoModeChange = (mode: 'translate' | 'rotate' | 'scale') => {
    setGizmoMode(mode);
    engine.setGizmoMode(mode);
  };

  return (
    <div className={styles.toolbar}>
      <div className={styles.section}>
        <span className={styles.label}>Transform:</span>
        <button
          className={`${styles.button} ${gizmoMode === 'translate' ? styles.active : ''}`}
          onClick={() => handleGizmoModeChange('translate')}
          title="Move (W)"
        >
          ⌖
        </button>
        <button
          className={`${styles.button} ${gizmoMode === 'rotate' ? styles.active : ''}`}
          onClick={() => handleGizmoModeChange('rotate')}
          title="Rotate (E)"
        >
          ↻
        </button>
        <button
          className={`${styles.button} ${gizmoMode === 'scale' ? styles.active : ''}`}
          onClick={() => handleGizmoModeChange('scale')}
          title="Scale (R)"
        >
          ▢
        </button>
      </div>
      
      <div className={styles.section}>
        <span className={styles.label}>Create:</span>
        <button className={styles.button} title="Create Empty Node">
          + Node
        </button>
        <button className={styles.button} title="Create CSG Cube">
          □ Cube
        </button>
      </div>
    </div>
  );
};

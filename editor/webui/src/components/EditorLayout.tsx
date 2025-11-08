/**
 * Moon Editor - Main Layout Component
 */

import React from 'react';
import { Toolbar } from './Toolbar';
import { Hierarchy } from './Hierarchy';
import { Inspector } from './Inspector';
import { Viewport } from './Viewport';
import styles from './EditorLayout.module.css';

export const EditorLayout: React.FC = () => {
  return (
    <div className={styles.container}>
      <div className={styles.toolbar}>
        <Toolbar />
      </div>
      <div className={styles.mainArea}>
        <div className={styles.leftPanel}>
          <Hierarchy />
        </div>
        <div className={styles.viewport}>
          <Viewport />
        </div>
        <div className={styles.rightPanel}>
          <Inspector />
        </div>
      </div>
    </div>
  );
};

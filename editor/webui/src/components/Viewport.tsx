/**
 * Moon Editor - 3D Viewport Component
 */

import React, { useEffect, useRef } from 'react';
import { isConnectedToEngine } from '@/utils/engine-bridge';
import styles from './Viewport.module.css';

export const Viewport: React.FC = () => {
  const canvasRef = useRef<HTMLDivElement>(null);
  const [isConnected, setIsConnected] = React.useState(false);

  useEffect(() => {
    setIsConnected(isConnectedToEngine());
  }, []);

  return (
    <div className={styles.viewport}>
      <div className={styles.header}>
        <span className={styles.title}>Viewport</span>
        <div className={styles.status}>
          <span className={`${styles.indicator} ${isConnected ? styles.connected : ''}`} />
          <span className={styles.statusText}>
            {isConnected ? 'Connected to Engine' : 'Mock Mode (Development)'}
          </span>
        </div>
      </div>
      <div ref={canvasRef} className={styles.canvas} id="viewport-container">
        {!isConnected && (
          <div className={styles.placeholder}>
            <div className={styles.placeholderContent}>
              <div className={styles.placeholderIcon}>📦</div>
              <div className={styles.placeholderText}>
                3D Viewport
                <br />
                <small>(CEF Native Rendering will be embedded here)</small>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

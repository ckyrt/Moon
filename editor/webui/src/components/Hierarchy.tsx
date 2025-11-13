/**
 * Moon Editor - Hierarchy Panel Component
 */

import React, { useEffect } from 'react';
import { useEditorStore } from '@/store/editorStore';
import { engine } from '@/utils/engine-bridge';
import type { SceneNode } from '@/types/engine';
import styles from './Hierarchy.module.css';

export const Hierarchy: React.FC = () => {
  const { scene, selectedNodeId, setSelectedNode, updateScene } = useEditorStore();

  // 从引擎加载场景数据（只在组件挂载时执行一次）
  useEffect(() => {
    const loadScene = async () => {
      try {
        console.log('[Hierarchy] Loading scene from engine...');
        const sceneData = await engine.getScene();
        console.log('[Hierarchy] Loaded scene from engine:', sceneData);
        updateScene(sceneData);
      } catch (error) {
        console.error('[Hierarchy] Failed to load scene:', error);
      }
    };

    loadScene();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []); // 只在挂载时执行一次

  const handleSelect = (nodeId: number) => {
    setSelectedNode(nodeId);
    engine.selectNode(nodeId);
  };

  const renderNode = (node: SceneNode, depth = 0) => {
    const isSelected = node.id === selectedNodeId;
    const hasChildren = node.children.length > 0;

    return (
      <div key={node.id} className={styles.nodeContainer}>
        <div
          className={`${styles.node} ${isSelected ? styles.selected : ''}`}
          style={{ paddingLeft: `${depth * 16 + 8}px` }}
          onClick={() => handleSelect(node.id)}
        >
          {hasChildren && <span className={styles.arrow}>▸</span>}
          <span className={styles.icon}>📦</span>
          <span className={styles.name}>{node.name}</span>
        </div>
        {hasChildren && (
          <div className={styles.children}>
            {node.children.map((childId) => {
              const child = scene.allNodes[childId];
              return child ? renderNode(child, depth + 1) : null;
            })}
          </div>
        )}
      </div>
    );
  };

  return (
    <div className={styles.hierarchy}>
      <div className={styles.header}>
        <span className={styles.title}>Hierarchy</span>
        <button className={styles.button} title="Create new node">
          +
        </button>
      </div>
      <div className={styles.content}>
        {!scene || !scene.rootNodes || scene.rootNodes.length === 0 ? (
          <div className={styles.empty}>No objects in scene</div>
        ) : (
          scene.rootNodes.map((nodeId) => {
            const node = scene.allNodes[nodeId];
            return node ? renderNode(node) : null;
          })
        )}
      </div>
    </div>
  );
};

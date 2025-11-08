/**
 * Moon Editor - Hierarchy Panel Component
 */

import React from 'react';
import { useEditorStore } from '@/store/editorStore';
import { engine } from '@/utils/engine-bridge';
import type { SceneNode } from '@/types/engine';
import styles from './Hierarchy.module.css';

export const Hierarchy: React.FC = () => {
  const { scene, selectedNodeId, setSelectedNode } = useEditorStore();

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
        {scene.rootNodes.length === 0 ? (
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

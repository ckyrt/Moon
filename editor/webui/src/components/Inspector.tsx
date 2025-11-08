/**
 * Moon Editor - Inspector Panel Component
 */

import React from 'react';
import { useEditorStore } from '@/store/editorStore';
import { engine } from '@/utils/engine-bridge';
import type { Vector3 } from '@/types/engine';
import styles from './Inspector.module.css';

export const Inspector: React.FC = () => {
  const { scene, selectedNodeId, updateNodeTransform } = useEditorStore();
  
  const selectedNode = selectedNodeId ? scene.allNodes[selectedNodeId] : null;

  const handlePositionChange = (axis: 'x' | 'y' | 'z', value: number) => {
    if (!selectedNode) return;
    const newPosition = { ...selectedNode.transform.position, [axis]: value };
    updateNodeTransform(selectedNode.id, { position: newPosition });
    engine.setPosition(selectedNode.id, newPosition);
  };

  const handleRotationChange = (axis: 'x' | 'y' | 'z', value: number) => {
    if (!selectedNode) return;
    const newRotation = { ...selectedNode.transform.rotation, [axis]: value };
    updateNodeTransform(selectedNode.id, { rotation: newRotation });
    engine.setRotation(selectedNode.id, newRotation);
  };

  const handleScaleChange = (axis: 'x' | 'y' | 'z', value: number) => {
    if (!selectedNode) return;
    const newScale = { ...selectedNode.transform.scale, [axis]: value };
    updateNodeTransform(selectedNode.id, { scale: newScale });
    engine.setScale(selectedNode.id, newScale);
  };

  if (!selectedNode) {
    return (
      <div className={styles.inspector}>
        <div className={styles.header}>Inspector</div>
        <div className={styles.empty}>No object selected</div>
      </div>
    );
  }

  return (
    <div className={styles.inspector}>
      <div className={styles.header}>Inspector</div>
      <div className={styles.content}>
        {/* Node Info */}
        <div className={styles.section}>
          <div className={styles.sectionTitle}>Node</div>
          <div className={styles.field}>
            <label>Name:</label>
            <input type="text" value={selectedNode.name} readOnly />
          </div>
          <div className={styles.field}>
            <label>ID:</label>
            <input type="text" value={selectedNode.id} readOnly />
          </div>
          <div className={styles.field}>
            <label>Active:</label>
            <input type="checkbox" checked={selectedNode.active} readOnly />
          </div>
        </div>

        {/* Transform */}
        <div className={styles.section}>
          <div className={styles.sectionTitle}>Transform</div>
          
          <div className={styles.vector3Field}>
            <label>Position:</label>
            <div className={styles.vector3Inputs}>
              <div className={styles.axisInput}>
                <span>X</span>
                <input
                  type="number"
                  value={selectedNode.transform.position.x.toFixed(2)}
                  onChange={(e) => handlePositionChange('x', parseFloat(e.target.value))}
                  step="0.1"
                />
              </div>
              <div className={styles.axisInput}>
                <span>Y</span>
                <input
                  type="number"
                  value={selectedNode.transform.position.y.toFixed(2)}
                  onChange={(e) => handlePositionChange('y', parseFloat(e.target.value))}
                  step="0.1"
                />
              </div>
              <div className={styles.axisInput}>
                <span>Z</span>
                <input
                  type="number"
                  value={selectedNode.transform.position.z.toFixed(2)}
                  onChange={(e) => handlePositionChange('z', parseFloat(e.target.value))}
                  step="0.1"
                />
              </div>
            </div>
          </div>

          <div className={styles.vector3Field}>
            <label>Rotation:</label>
            <div className={styles.vector3Inputs}>
              <div className={styles.axisInput}>
                <span>X</span>
                <input
                  type="number"
                  value={selectedNode.transform.rotation.x.toFixed(1)}
                  onChange={(e) => handleRotationChange('x', parseFloat(e.target.value))}
                  step="1"
                />
              </div>
              <div className={styles.axisInput}>
                <span>Y</span>
                <input
                  type="number"
                  value={selectedNode.transform.rotation.y.toFixed(1)}
                  onChange={(e) => handleRotationChange('y', parseFloat(e.target.value))}
                  step="1"
                />
              </div>
              <div className={styles.axisInput}>
                <span>Z</span>
                <input
                  type="number"
                  value={selectedNode.transform.rotation.z.toFixed(1)}
                  onChange={(e) => handleRotationChange('z', parseFloat(e.target.value))}
                  step="1"
                />
              </div>
            </div>
          </div>

          <div className={styles.vector3Field}>
            <label>Scale:</label>
            <div className={styles.vector3Inputs}>
              <div className={styles.axisInput}>
                <span>X</span>
                <input
                  type="number"
                  value={selectedNode.transform.scale.x.toFixed(2)}
                  onChange={(e) => handleScaleChange('x', parseFloat(e.target.value))}
                  step="0.1"
                />
              </div>
              <div className={styles.axisInput}>
                <span>Y</span>
                <input
                  type="number"
                  value={selectedNode.transform.scale.y.toFixed(2)}
                  onChange={(e) => handleScaleChange('y', parseFloat(e.target.value))}
                  step="0.1"
                />
              </div>
              <div className={styles.axisInput}>
                <span>Z</span>
                <input
                  type="number"
                  value={selectedNode.transform.scale.z.toFixed(2)}
                  onChange={(e) => handleScaleChange('z', parseFloat(e.target.value))}
                  step="0.1"
                />
              </div>
            </div>
          </div>
        </div>

        {/* Components */}
        {selectedNode.components.length > 0 && (
          <div className={styles.section}>
            <div className={styles.sectionTitle}>Components</div>
            {selectedNode.components.map((comp, idx) => (
              <div key={idx} className={styles.component}>
                <span className={styles.componentType}>{comp.type}</span>
                <span className={styles.componentStatus}>
                  {comp.enabled ? '✓' : '✗'}
                </span>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

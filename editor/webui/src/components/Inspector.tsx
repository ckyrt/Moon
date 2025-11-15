/**
 * Moon Editor - Inspector Panel Component
 * 显示选中节点的属性并支持编辑
 */

import React from 'react';
import { useEditorStore } from '@/store/editorStore';
import { engine } from '@/utils/engine-bridge';
import type { Vector3 } from '@/types/engine';
import styles from './Inspector.module.css';

export const Inspector: React.FC = () => {
  const { scene, selectedNodeId, updateNodeTransform } = useEditorStore();
  const selectedNode = selectedNodeId ? scene.allNodes[selectedNodeId] : null;

  // ========== Transform 修改处理器 ==========
  
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

  // ========== 渲染 ==========

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
        
        {/* 节点信息 */}
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

        {/* Transform 组件 */}
        <div className={styles.section}>
          <div className={styles.sectionTitle}>Transform</div>
          
          {/* Position */}
          <Vector3Input
            label="Position"
            value={selectedNode.transform.position}
            onChange={handlePositionChange}
            step={0.1}
            precision={2}
          />

          {/* Rotation */}
          <Vector3Input
            label="Rotation"
            value={selectedNode.transform.rotation}
            onChange={handleRotationChange}
            step={1}
            precision={1}
          />

          {/* Scale */}
          <Vector3Input
            label="Scale"
            value={selectedNode.transform.scale}
            onChange={handleScaleChange}
            step={0.1}
            precision={2}
          />
        </div>

        {/* 组件列表 */}
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

// ========== Vector3 输入组件 ==========

interface Vector3InputProps {
  label: string;
  value: Vector3;
  onChange: (axis: 'x' | 'y' | 'z', value: number) => void;
  step: number;
  precision: number;
}

const Vector3Input: React.FC<Vector3InputProps> = ({ label, value, onChange, step, precision }) => {
  return (
    <div className={styles.vector3Field}>
      <label>{label}:</label>
      <div className={styles.vector3Inputs}>
        {(['x', 'y', 'z'] as const).map((axis) => (
          <div key={axis} className={styles.axisInput}>
            <span>{axis.toUpperCase()}</span>
            <input
              type="number"
              value={value[axis].toFixed(precision)}
              onChange={(e) => onChange(axis, parseFloat(e.target.value))}
              step={step}
            />
          </div>
        ))}
      </div>
    </div>
  );
};


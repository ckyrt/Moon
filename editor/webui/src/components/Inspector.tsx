/**
 * Moon Editor - Inspector Panel Component
 * 显示选中节点的属性并支持编辑
 */

import React from 'react';
import { useEditorStore } from '@/store/editorStore';
import { eulerToQuaternion } from '@/utils/math';
import { getUndoManager } from '@/undo';
import { SetPositionCommand, SetRotationCommand, SetScaleCommand } from '@/undo';
import type { Vector3, Component, MeshRendererComponent, RigidBodyComponent } from '@/types/engine';
import styles from './Inspector.module.css';

export const Inspector: React.FC = () => {
  const { scene, selectedNodeId } = useEditorStore();
  const selectedNode = selectedNodeId ? scene.allNodes[selectedNodeId] : null;

  // 获取 UndoManager 单例
  const undoManager = getUndoManager({ debug: true });

  // ========== Transform 修改处理器 ==========
  
  const handlePositionChange = (axis: 'x' | 'y' | 'z', value: number) => {
    if (!selectedNode) return;
    
    const oldPosition = { ...selectedNode.transform.position };
    const newPosition = { ...oldPosition, [axis]: value };
    
    // 使用 Command 执行修改
    const command = new SetPositionCommand(selectedNode.id, oldPosition, newPosition);
    undoManager.execute(command);
  };

  const handleRotationChange = (axis: 'x' | 'y' | 'z', value: number) => {
    if (!selectedNode) return;
    
    const oldRotation = { ...selectedNode.transform.rotation };
    const newRotation = { ...oldRotation, [axis]: value };
    
    // 将欧拉角转换为四元数
    const oldQuaternion = eulerToQuaternion(oldRotation);
    const newQuaternion = eulerToQuaternion(newRotation);
    
    // 使用 Command 执行修改
    const command = new SetRotationCommand(
      selectedNode.id,
      oldRotation,
      newRotation,
      oldQuaternion,
      newQuaternion
    );
    undoManager.execute(command);
  };

  const handleScaleChange = (axis: 'x' | 'y' | 'z', value: number) => {
    if (!selectedNode) return;
    
    const oldScale = { ...selectedNode.transform.scale };
    const newScale = { ...oldScale, [axis]: value };
    
    // 使用 Command 执行修改
    const command = new SetScaleCommand(selectedNode.id, oldScale, newScale);
    undoManager.execute(command);
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
              <ComponentView key={idx} component={comp} />
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

// ========== 组件显示组件 ==========

const ComponentView: React.FC<{ component: Component }> = ({ component }) => {
  const renderComponentDetails = () => {
    switch (component.type) {
      case 'MeshRenderer':
        const meshRenderer = component as MeshRendererComponent;
        return (
          <div className={styles.componentDetails}>
            <div className={styles.propertyRow}>
              <span>Visible:</span>
              <span>{meshRenderer.visible ? 'Yes' : 'No'}</span>
            </div>
            <div className={styles.propertyRow}>
              <span>Has Mesh:</span>
              <span>{meshRenderer.hasMesh ? 'Yes' : 'No'}</span>
            </div>
          </div>
        );
      
      case 'RigidBody':
        const rigidBody = component as RigidBodyComponent;
        return (
          <div className={styles.componentDetails}>
            <div className={styles.propertyRow}>
              <span>Shape:</span>
              <span className={styles.badge}>{rigidBody.shapeType}</span>
            </div>
            <div className={styles.propertyRow}>
              <span>Mass:</span>
              <span>{rigidBody.mass.toFixed(2)} kg</span>
            </div>
            <div className={styles.propertyRow}>
              <span>Size:</span>
              <span>({rigidBody.size.map(v => v.toFixed(2)).join(', ')})</span>
            </div>
            {rigidBody.linearVelocity && (
              <div className={styles.propertyRow}>
                <span>Linear Vel:</span>
                <span>({rigidBody.linearVelocity.map(v => v.toFixed(2)).join(', ')})</span>
              </div>
            )}
            {rigidBody.angularVelocity && (
              <div className={styles.propertyRow}>
                <span>Angular Vel:</span>
                <span>({rigidBody.angularVelocity.map(v => v.toFixed(2)).join(', ')})</span>
              </div>
            )}
            <div className={styles.propertyRow}>
              <span>Has Body:</span>
              <span>{rigidBody.hasBody ? '✓ Active' : '✗ Inactive'}</span>
            </div>
          </div>
        );
      
      default:
        return null;
    }
  };

  return (
    <div className={styles.component}>
      <div className={styles.componentHeader}>
        <span className={styles.componentType}>{component.type}</span>
        <span className={styles.componentStatus}>
          {component.enabled ? '✓' : '✗'}
        </span>
      </div>
      {renderComponentDetails()}
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


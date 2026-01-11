/**
 * Moon Editor - Inspector Panel Component
 * 显示选中节点的属性并支持编辑
 */

import React, { useState } from 'react';
import { logger } from '@/utils/logger';
import { useEditorStore } from '@/store/editorStore';
import { eulerToQuaternion } from '@/utils/math';
import { getUndoManager } from '@/undo';
import { engine } from '@/utils/engine-bridge';
import { 
  SetPositionCommand, 
  SetRotationCommand, 
  SetScaleCommand,
  RenameNodeCommand,
  SetNodeActiveCommand,
  SetLightColorCommand,
  SetLightIntensityCommand,
  SetLightRangeCommand,
  SetLightTypeCommand,
  SetSkyboxIntensityCommand,
  SetSkyboxRotationCommand,
  SetSkyboxTintCommand,
  SetSkyboxIBLCommand,
  SetSkyboxEnvironmentMapCommand,
  SetMaterialMetallicCommand,
  SetMaterialRoughnessCommand,
  SetMaterialBaseColorCommand,
  SetMaterialTextureCommand
} from '@/undo';
import type { Vector3, Component, MeshRendererComponent, RigidBodyComponent, LightComponent, SkyboxComponent, MaterialComponent } from '@/types/engine';
import styles from './Inspector.module.css';

export const Inspector: React.FC = () => {
  const { scene, selectedNodeId } = useEditorStore();
  const selectedNode = selectedNodeId ? scene.allNodes[selectedNodeId] : null;

  // 获取 UndoManager 单例
  const undoManager = getUndoManager({ debug: true });

  // ========== Transform 修改处理器 ==========
  
  const handleNameChange = (newName: string) => {
    if (!selectedNode || newName === selectedNode.name) return;
    
    const command = new RenameNodeCommand(selectedNode.id, selectedNode.name, newName);
    undoManager.execute(command);
  };

  const handleActiveChange = (newActive: boolean) => {
    if (!selectedNode) return;
    
    const command = new SetNodeActiveCommand(selectedNode.id, selectedNode.active, newActive);
    undoManager.execute(command);
  };
  
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
            <input 
              type="text" 
              value={selectedNode.name} 
              onChange={(e) => handleNameChange(e.target.value)}
              onBlur={(e) => handleNameChange(e.target.value)}
            />
          </div>
          <div className={styles.field}>
            <label>ID:</label>
            <input type="text" value={selectedNode.id} readOnly />
          </div>
          <div className={styles.field}>
            <label>Active:</label>
            <input 
              type="checkbox" 
              checked={selectedNode.active} 
              onChange={(e) => handleActiveChange(e.target.checked)}
            />
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
      
      case 'Light':
        return <LightEditor component={component as LightComponent} />;
      
      case 'Skybox':
        return <SkyboxEditor component={component as SkyboxComponent} />;
      
      case 'Material':
        return <MaterialEditor component={component as MaterialComponent} />;
      
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

// ========== Light 编辑组件 ==========

interface LightEditorProps {
  component: LightComponent;
}

const LightEditor: React.FC<LightEditorProps> = ({ component }) => {
  const selectedNode = useEditorStore((state) => 
    state.selectedNodeId ? state.scene.allNodes[state.selectedNodeId] : null
  );
  const undoManager = getUndoManager();
  const [showTypeDropdown, setShowTypeDropdown] = useState(false);
  const [showColorPicker, setShowColorPicker] = useState(false);

  if (!selectedNode) return null;

  const handleLightTypeChange = (newType: 'Directional' | 'Point' | 'Spot') => {
    console.log('[LightEditor] Type change:', component.lightType, '->', newType);
    const command = new SetLightTypeCommand(selectedNode.id, component.lightType, newType);
    undoManager.execute(command);
    setShowTypeDropdown(false);
  };

  // RGB to Hex
  const rgbToHex = (r: number, g: number, b: number): string => {
    const toHex = (v: number) => {
      const hex = Math.round(Math.max(0, Math.min(1, v)) * 255).toString(16);
      return hex.padStart(2, '0');
    };
    return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
  };

  // Hex to RGB
  const hexToRgb = (hex: string): { x: number; y: number; z: number } => {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    if (result) {
      return {
        x: parseInt(result[1], 16) / 255,
        y: parseInt(result[2], 16) / 255,
        z: parseInt(result[3], 16) / 255,
      };
    }
    return { x: 1, y: 1, z: 1 };
  };

  const handleColorClick = (r: number, g: number, b: number) => {
    const oldColor = { x: component.color[0], y: component.color[1], z: component.color[2] };
    const newColor = { x: r, y: g, z: b };
    const command = new SetLightColorCommand(selectedNode.id, oldColor, newColor);
    undoManager.execute(command);
  };

  const handleIntensityChange = (value: number) => {
    if (isNaN(value)) return;
    const command = new SetLightIntensityCommand(selectedNode.id, component.intensity, value);
    undoManager.execute(command);
  };

  const handleRangeChange = (value: number) => {
    if (isNaN(value)) return;
    const command = new SetLightRangeCommand(selectedNode.id, component.range || 10, value);
    undoManager.execute(command);
  };

  return (
    <div className={styles.componentDetails} style={{ position: 'relative', zIndex: 100 }}>
      {/* Light Type - Custom Dropdown */}
      <div className={styles.propertyRow} style={{ flexDirection: 'column', alignItems: 'stretch', gap: '4px' }}>
        <label>Type:</label>
        <div style={{ position: 'relative' }}>
          <div
            onClick={() => setShowTypeDropdown(!showTypeDropdown)}
            style={{
              padding: '6px 24px 6px 8px',
              borderRadius: '4px',
              border: '1px solid var(--border-color)',
              background: 'var(--bg-tertiary)',
              cursor: 'pointer',
              position: 'relative',
              userSelect: 'none'
            }}
          >
            {component.lightType}
            <span style={{ position: 'absolute', right: '8px', top: '50%', transform: 'translateY(-50%)' }}>▼</span>
          </div>
          {showTypeDropdown && (
            <div style={{
              position: 'absolute',
              top: '100%',
              left: 0,
              right: 0,
              background: 'var(--bg-secondary)',
              border: '1px solid var(--border-color)',
              borderRadius: '4px',
              marginTop: '2px',
              zIndex: 1000,
              boxShadow: '0 4px 8px rgba(0,0,0,0.3)'
            }}>
              {(['Directional', 'Point', 'Spot'] as const).map((type) => (
                <div
                  key={type}
                  onClick={() => handleLightTypeChange(type)}
                  style={{
                    padding: '6px 8px',
                    cursor: 'pointer',
                    background: component.lightType === type ? 'var(--accent-blue)' : 'transparent'
                  }}
                  onMouseEnter={(e) => {
                    if (component.lightType !== type) {
                      e.currentTarget.style.background = 'var(--bg-tertiary)';
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (component.lightType !== type) {
                      e.currentTarget.style.background = 'transparent';
                    }
                  }}
                >
                  {type}
                </div>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* Color - Custom Color Picker */}
      <div className={styles.propertyRow} style={{ flexDirection: 'column', alignItems: 'stretch', gap: '4px' }}>
        <label>Color:</label>
        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
          <div
            onClick={() => setShowColorPicker(!showColorPicker)}
            style={{
              width: '60px',
              height: '32px',
              borderRadius: '4px',
              border: '1px solid var(--border-color)',
              cursor: 'pointer',
              background: rgbToHex(component.color[0], component.color[1], component.color[2]),
              flexShrink: 0,
              position: 'relative'
            }}
          />
          <span style={{ fontSize: '11px', color: 'var(--text-secondary)', fontFamily: 'monospace' }}>
            ({(component.color[0] * 255).toFixed(0)}, {(component.color[1] * 255).toFixed(0)}, {(component.color[2] * 255).toFixed(0)})
          </span>
        </div>
        {showColorPicker && (
          <div style={{
            padding: '8px',
            background: 'var(--bg-secondary)',
            border: '1px solid var(--border-color)',
            borderRadius: '4px',
            marginTop: '4px'
          }}>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(8, 1fr)', gap: '4px', marginBottom: '8px' }}>
              {[
                [1, 1, 1], [0.9, 0.9, 0.9], [0.7, 0.7, 0.7], [0.5, 0.5, 0.5],
                [1, 0, 0], [1, 0.5, 0], [1, 1, 0], [0.5, 1, 0],
                [0, 1, 0], [0, 1, 0.5], [0, 1, 1], [0, 0.5, 1],
                [0, 0, 1], [0.5, 0, 1], [1, 0, 1], [1, 0, 0.5]
              ].map((rgb, i) => (
                <div
                  key={i}
                  onClick={() => {
                    handleColorClick(rgb[0], rgb[1], rgb[2]);
                    setShowColorPicker(false);
                  }}
                  style={{
                    width: '100%',
                    paddingBottom: '100%',
                    background: rgbToHex(rgb[0], rgb[1], rgb[2]),
                    borderRadius: '2px',
                    cursor: 'pointer',
                    border: '1px solid var(--border-color)'
                  }}
                />
              ))}
            </div>
            <button
              onClick={() => setShowColorPicker(false)}
              style={{
                width: '100%',
                padding: '4px',
                background: 'var(--bg-tertiary)',
                border: '1px solid var(--border-color)',
                borderRadius: '4px',
                cursor: 'pointer',
                fontSize: '11px'
              }}
            >
              Close
            </button>
          </div>
        )}
      </div>

      {/* Intensity */}
      <div className={styles.propertyRow}>
        <label style={{ minWidth: '70px' }}>Intensity:</label>
        <input
          type="number"
          value={component.intensity}
          onChange={(e) => handleIntensityChange(parseFloat(e.target.value))}
          onBlur={(e) => handleIntensityChange(parseFloat(e.target.value))}
          step={0.1}
          min={0}
          style={{ 
            flex: 1,
            padding: '4px 8px', 
            borderRadius: '4px', 
            border: '1px solid var(--border-color)', 
            background: 'var(--bg-tertiary)'
          }}
        />
      </div>

      {/* Range (for Point and Spot lights) */}
      {component.lightType !== 'Directional' && (
        <div className={styles.propertyRow}>
          <label style={{ minWidth: '70px' }}>Range:</label>
          <input
            type="number"
            value={component.range || 10}
            onChange={(e) => handleRangeChange(parseFloat(e.target.value))}
            onBlur={(e) => handleRangeChange(parseFloat(e.target.value))}
            step={0.5}
            min={0.1}
            style={{ 
              flex: 1,
              padding: '4px 8px', 
              borderRadius: '4px', 
              border: '1px solid var(--border-color)', 
              background: 'var(--bg-tertiary)'
            }}
          />
        </div>
      )}
    </div>
  );
};

// ========== Skybox 编辑组件 ==========

interface SkyboxEditorProps {
  component: SkyboxComponent;
}

const SkyboxEditor: React.FC<SkyboxEditorProps> = ({ component }) => {
  const selectedNode = useEditorStore((state) => 
    state.selectedNodeId ? state.scene.allNodes[state.selectedNodeId] : null
  );
  const undoManager = getUndoManager();
  const [showTintPicker, setShowTintPicker] = useState(false);

  if (!selectedNode) return null;

  const handleIntensityChange = (value: number) => {
    if (isNaN(value)) return;
    const command = new SetSkyboxIntensityCommand(selectedNode.id, component.intensity, value);
    undoManager.execute(command);
  };

  const handleRotationChange = (value: number) => {
    if (isNaN(value)) return;
    const command = new SetSkyboxRotationCommand(selectedNode.id, component.rotation, value);
    undoManager.execute(command);
  };

  const handleTintClick = (r: number, g: number, b: number) => {
    const oldTint = { x: component.tint[0], y: component.tint[1], z: component.tint[2] };
    const newTint = { x: r, y: g, z: b };
    const command = new SetSkyboxTintCommand(selectedNode.id, oldTint, newTint);
    undoManager.execute(command);
  };

  const handleIBLChange = (enabled: boolean) => {
    const command = new SetSkyboxIBLCommand(selectedNode.id, component.enableIBL, enabled);
    undoManager.execute(command);
  };

  const handlePathChange = (path: string) => {
    if (path === component.environmentMapPath) return;
    const command = new SetSkyboxEnvironmentMapCommand(selectedNode.id, component.environmentMapPath, path);
    undoManager.execute(command);
  };

  const rgbToHex = (r: number, g: number, b: number): string => {
    const toHex = (v: number) => {
      const hex = Math.round(Math.max(0, Math.min(1, v)) * 255).toString(16);
      return hex.padStart(2, '0');
    };
    return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
  };

  return (
    <div className={styles.componentDetails} style={{ position: 'relative', zIndex: 100 }}>
      {/* Environment Map Path */}
      <div className={styles.propertyRow} style={{ flexDirection: 'column', alignItems: 'stretch', gap: '4px' }}>
        <label>Environment Map:</label>
        <input
          type="text"
          value={component.environmentMapPath}
          onChange={(e) => handlePathChange(e.target.value)}
          onBlur={(e) => handlePathChange(e.target.value)}
          placeholder="assets/textures/environment.hdr"
          style={{ 
            flex: 1,
            padding: '4px 8px', 
            borderRadius: '4px', 
            border: '1px solid var(--border-color)', 
            background: 'var(--bg-tertiary)',
            fontSize: '11px',
            fontFamily: 'monospace'
          }}
        />
      </div>

      {/* Intensity */}
      <div className={styles.propertyRow}>
        <label style={{ minWidth: '70px' }}>Intensity:</label>
        <input
          type="number"
          value={component.intensity}
          onChange={(e) => handleIntensityChange(parseFloat(e.target.value))}
          onBlur={(e) => handleIntensityChange(parseFloat(e.target.value))}
          step={0.1}
          min={0}
          style={{ 
            flex: 1,
            padding: '4px 8px', 
            borderRadius: '4px', 
            border: '1px solid var(--border-color)', 
            background: 'var(--bg-tertiary)'
          }}
        />
      </div>

      {/* Rotation */}
      <div className={styles.propertyRow}>
        <label style={{ minWidth: '70px' }}>Rotation:</label>
        <input
          type="number"
          value={component.rotation}
          onChange={(e) => handleRotationChange(parseFloat(e.target.value))}
          onBlur={(e) => handleRotationChange(parseFloat(e.target.value))}
          step={1}
          min={0}
          max={360}
          style={{ 
            flex: 1,
            padding: '4px 8px', 
            borderRadius: '4px', 
            border: '1px solid var(--border-color)', 
            background: 'var(--bg-tertiary)'
          }}
        />
      </div>

      {/* Tint - Custom Color Picker */}
      <div className={styles.propertyRow} style={{ flexDirection: 'column', alignItems: 'stretch', gap: '4px' }}>
        <label>Tint:</label>
        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
          <div
            onClick={() => setShowTintPicker(!showTintPicker)}
            style={{
              width: '60px',
              height: '32px',
              borderRadius: '4px',
              border: '1px solid var(--border-color)',
              cursor: 'pointer',
              background: rgbToHex(component.tint[0], component.tint[1], component.tint[2]),
              flexShrink: 0,
              position: 'relative'
            }}
          />
          <span style={{ fontSize: '11px', color: 'var(--text-secondary)', fontFamily: 'monospace' }}>
            ({(component.tint[0] * 255).toFixed(0)}, {(component.tint[1] * 255).toFixed(0)}, {(component.tint[2] * 255).toFixed(0)})
          </span>
        </div>
        {showTintPicker && (
          <div style={{
            padding: '8px',
            background: 'var(--bg-secondary)',
            border: '1px solid var(--border-color)',
            borderRadius: '4px',
            marginTop: '4px'
          }}>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(8, 1fr)', gap: '4px', marginBottom: '8px' }}>
              {[
                [1, 1, 1], [0.9, 0.9, 0.9], [0.7, 0.7, 0.7], [0.5, 0.5, 0.5],
                [1, 0, 0], [1, 0.5, 0], [1, 1, 0], [0.5, 1, 0],
                [0, 1, 0], [0, 1, 0.5], [0, 1, 1], [0, 0.5, 1],
                [0, 0, 1], [0.5, 0, 1], [1, 0, 1], [1, 0, 0.5]
              ].map((rgb, i) => (
                <div
                  key={i}
                  onClick={() => {
                    handleTintClick(rgb[0], rgb[1], rgb[2]);
                    setShowTintPicker(false);
                  }}
                  style={{
                    width: '100%',
                    paddingBottom: '100%',
                    background: rgbToHex(rgb[0], rgb[1], rgb[2]),
                    borderRadius: '2px',
                    cursor: 'pointer',
                    border: '1px solid var(--border-color)'
                  }}
                />
              ))}
            </div>
            <button
              onClick={() => setShowTintPicker(false)}
              style={{
                width: '100%',
                padding: '4px',
                background: 'var(--bg-tertiary)',
                border: '1px solid var(--border-color)',
                borderRadius: '4px',
                cursor: 'pointer',
                fontSize: '11px'
              }}
            >
              Close
            </button>
          </div>
        )}
      </div>

      {/* Enable IBL */}
      <div className={styles.propertyRow}>
        <label style={{ minWidth: '70px' }}>Enable IBL:</label>
        <input
          type="checkbox"
          checked={component.enableIBL}
          onChange={(e) => handleIBLChange(e.target.checked)}
          style={{ 
            width: '16px',
            height: '16px',
            cursor: 'pointer'
          }}
        />
      </div>
    </div>
  );
};
// ========== Material 编辑组件 ==========

interface MaterialEditorProps {
  component: MaterialComponent;
}

const MaterialEditor: React.FC<MaterialEditorProps> = ({ component }) => {
  const selectedNode = useEditorStore((state) => 
    state.selectedNodeId ? state.scene.allNodes[state.selectedNodeId] : null
  );
  const undoManager = getUndoManager();
  const [showColorPicker, setShowColorPicker] = useState(false);
  const [dropdownOpen, setDropdownOpen] = useState(false);

  if (!selectedNode) return null;

  const presetOptions = [
    { value: 'None', label: 'None (Custom)' },
    { value: 'Concrete', label: 'Concrete' },
    { value: 'Fabric', label: 'Fabric' },
    { value: 'Metal', label: 'Metal' },
    { value: 'Plastic', label: 'Plastic' },
    { value: 'Rock', label: 'Rock' },
    { value: 'Wood', label: 'Wood' },
    { value: 'Glass', label: 'Glass' },
    { value: 'PolishedMetal', label: 'Polished Metal' },
  ];

  const handlePresetChange = async (preset: string) => {
    if (!selectedNode) {
      logger.error('MaterialEditor', 'handlePresetChange: selectedNode is null');
      return;
    }
    
    logger.info('MaterialEditor', `handlePresetChange called with preset: ${preset}`);
    
    try {
      // 立即更新UI显示的preset值
      component.preset = preset;
      
      // 发送给后端，后端会自己处理加载对应的贴图
      logger.info('MaterialEditor', `Calling engine.setMaterialPreset, nodeId: ${selectedNode.id}, preset: ${preset}`);
      await engine.setMaterialPreset(selectedNode.id, preset);
      logger.info('MaterialEditor', 'setMaterialPreset completed');
    } catch (error) {
      logger.error('MaterialEditor', 'Failed to set preset', { error: String(error) });
    }
  };

  const handleMetallicChange = (value: number) => {
    if (isNaN(value)) return;
    const command = new SetMaterialMetallicCommand(selectedNode.id, component.metallic, value);
    undoManager.execute(command);
  };

  const handleRoughnessChange = (value: number) => {
    if (isNaN(value)) return;
    const command = new SetMaterialRoughnessCommand(selectedNode.id, component.roughness, value);
    undoManager.execute(command);
  };

  const handleBaseColorChange = (r: number, g: number, b: number) => {
    const oldColor = { x: component.baseColor[0], y: component.baseColor[1], z: component.baseColor[2] };
    const newColor = { x: r, y: g, z: b };
    const command = new SetMaterialBaseColorCommand(selectedNode.id, oldColor, newColor);
    undoManager.execute(command);
  };

  // RGB to Hex
  const rgbToHex = (r: number, g: number, b: number): string => {
    const toHex = (v: number) => {
      const hex = Math.round(Math.max(0, Math.min(1, v)) * 255).toString(16);
      return hex.padStart(2, '0');
    };
    return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
  };

  const handleColorClick = (r: number, g: number, b: number) => {
    handleBaseColorChange(r, g, b);
    setShowColorPicker(false);
  };

  // 常用颜色预设
  const colorPresets: [number, number, number][] = [
    [1, 1, 1],      // White
    [0.8, 0.8, 0.8], // Light Gray
    [0.5, 0.5, 0.5], // Gray
    [0.2, 0.2, 0.2], // Dark Gray
    [1, 0, 0],      // Red
    [0, 1, 0],      // Green
    [0, 0, 1],      // Blue
    [1, 1, 0],      // Yellow
    [1, 0, 1],      // Magenta
    [0, 1, 1],      // Cyan
    [1, 0.5, 0],    // Orange
    [0.5, 0, 0.5],  // Purple
  ];

  return (
    <div className={styles.componentDetails}>
      {/* Material Preset Dropdown - Custom implementation for CEF OSR */}
      <div className={styles.propertyRow}>
        <label style={{ minWidth: '70px' }}>Preset:</label>
        <div style={{ flex: 1, position: 'relative' }}>
          <div
            onClick={(e) => {
              e.stopPropagation();
              logger.info('MaterialEditor', 'Dropdown header clicked');
              setDropdownOpen(!dropdownOpen);
            }}
            style={{
              padding: '4px 8px',
              borderRadius: '4px',
              border: '1px solid var(--border-color)',
              background: 'var(--bg-tertiary)',
              fontSize: '11px',
              cursor: 'pointer',
              userSelect: 'none',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center'
            }}
          >
            <span>{presetOptions.find(o => o.value === (component.preset || 'None'))?.label}</span>
            <span style={{ fontSize: '10px' }}>▼</span>
          </div>
          {dropdownOpen && (
            <div
              style={{
                position: 'absolute',
                top: '100%',
                left: 0,
                right: 0,
                marginTop: '2px',
                background: 'var(--bg-tertiary)',
                border: '1px solid var(--border-color)',
                borderRadius: '4px',
                maxHeight: '200px',
                overflowY: 'auto',
                zIndex: 1000,
                boxShadow: '0 2px 8px rgba(0,0,0,0.3)'
              }}
            >
              {presetOptions.map(option => (
                <div
                  key={option.value}
                  onClick={(e) => {
                    e.stopPropagation();
                    logger.info('MaterialEditor', `Option clicked: ${option.value}`);
                    handlePresetChange(option.value);
                    setDropdownOpen(false);
                  }}
                  style={{
                    padding: '6px 8px',
                    fontSize: '11px',
                    cursor: 'pointer',
                    background: (component.preset || 'None') === option.value ? 'var(--accent-color)' : 'transparent',
                    color: (component.preset || 'None') === option.value ? 'white' : 'inherit'
                  }}
                  onMouseEnter={(e) => {
                    if ((component.preset || 'None') !== option.value) {
                      e.currentTarget.style.background = 'var(--bg-secondary)';
                    }
                  }}
                  onMouseLeave={(e) => {
                    if ((component.preset || 'None') !== option.value) {
                      e.currentTarget.style.background = 'transparent';
                    }
                  }}
                >
                  {option.label}
                </div>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* Metallic */}
      <div className={styles.propertyRow}>
        <label style={{ minWidth: '70px' }}>Metallic:</label>
        <input
          type="range"
          min="0"
          max="1"
          step="0.01"
          value={component.metallic}
          onChange={(e) => handleMetallicChange(parseFloat(e.target.value))}
          style={{ flex: 1, marginRight: '8px' }}
        />
        <input
          type="number"
          value={component.metallic.toFixed(2)}
          onChange={(e) => handleMetallicChange(parseFloat(e.target.value))}
          step={0.01}
          min={0}
          max={1}
          style={{ 
            width: '60px',
            padding: '4px 8px', 
            borderRadius: '4px', 
            border: '1px solid var(--border-color)', 
            background: 'var(--bg-tertiary)'
          }}
        />
      </div>

      {/* Roughness */}
      <div className={styles.propertyRow}>
        <label style={{ minWidth: '70px' }}>Roughness:</label>
        <input
          type="range"
          min="0"
          max="1"
          step="0.01"
          value={component.roughness}
          onChange={(e) => handleRoughnessChange(parseFloat(e.target.value))}
          style={{ flex: 1, marginRight: '8px' }}
        />
        <input
          type="number"
          value={component.roughness.toFixed(2)}
          onChange={(e) => handleRoughnessChange(parseFloat(e.target.value))}
          step={0.01}
          min={0}
          max={1}
          style={{ 
            width: '60px',
            padding: '4px 8px', 
            borderRadius: '4px', 
            border: '1px solid var(--border-color)', 
            background: 'var(--bg-tertiary)'
          }}
        />
      </div>

      {/* Base Color */}
      <div className={styles.propertyRow} style={{ position: 'relative' }}>
        <label style={{ minWidth: '70px' }}>Base Color:</label>
        <div
          onClick={() => setShowColorPicker(!showColorPicker)}
          style={{
            width: '40px',
            height: '24px',
            background: rgbToHex(component.baseColor[0], component.baseColor[1], component.baseColor[2]),
            borderRadius: '4px',
            cursor: 'pointer',
            border: '1px solid var(--border-color)'
          }}
        />
        <span style={{ marginLeft: '8px', fontSize: '11px', color: 'var(--text-secondary)' }}>
          RGB({component.baseColor[0].toFixed(2)}, {component.baseColor[1].toFixed(2)}, {component.baseColor[2].toFixed(2)})
        </span>
        
        {/* Color Picker Dropdown */}
        {showColorPicker && (
          <div style={{
            position: 'absolute',
            top: '100%',
            left: '70px',
            zIndex: 1000,
            background: 'var(--bg-secondary)',
            border: '1px solid var(--border-color)',
            borderRadius: '4px',
            padding: '8px',
            marginTop: '4px',
            boxShadow: '0 2px 8px rgba(0,0,0,0.3)'
          }}>
            <div style={{ 
              display: 'grid', 
              gridTemplateColumns: 'repeat(4, 1fr)', 
              gap: '6px',
              marginBottom: '8px'
            }}>
              {colorPresets.map((rgb, i) => (
                <div
                  key={i}
                  onClick={() => handleColorClick(rgb[0], rgb[1], rgb[2])}
                  style={{
                    width: '24px',
                    height: '24px',
                    background: rgbToHex(rgb[0], rgb[1], rgb[2]),
                    borderRadius: '2px',
                    cursor: 'pointer',
                    border: '1px solid var(--border-color)'
                  }}
                />
              ))}
            </div>
            <button
              onClick={() => setShowColorPicker(false)}
              style={{
                width: '100%',
                padding: '4px',
                background: 'var(--bg-tertiary)',
                border: '1px solid var(--border-color)',
                borderRadius: '4px',
                cursor: 'pointer',
                fontSize: '11px'
              }}
            >
              Close
            </button>
          </div>
        )}
      </div>
    </div>
  );
};
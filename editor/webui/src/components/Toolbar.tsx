/**
 * Moon Editor - Toolbar Component
 */

import React, { useState, useRef, useEffect } from 'react';
import { useEditorStore } from '@/store/editorStore';
import { engine } from '@/utils/engine-bridge';
import { getUndoManager, CreatePrimitiveCommand } from '@/undo';
import styles from './Toolbar.module.css';

export const Toolbar: React.FC = () => {
  const { gizmoMode, setGizmoMode } = useEditorStore();
  const [showCreateMenu, setShowCreateMenu] = useState(false);
  const [showCSGMenu, setShowCSGMenu] = useState(false);
  const [coordinateMode, setCoordinateMode] = useState<'world' | 'local'>('world');  // 🎯 World/Local 模式
  const menuRef = useRef<HTMLDivElement>(null);
  const csgMenuRef = useRef<HTMLDivElement>(null);

  const handleGizmoModeChange = (mode: 'translate' | 'rotate' | 'scale') => {
    setGizmoMode(mode);
    engine.setGizmoMode(mode);
  };

  // 🎯 切换 World/Local 坐标系模式
  const handleCoordinateModeToggle = async () => {
    const newMode = coordinateMode === 'world' ? 'local' : 'world';
    setCoordinateMode(newMode);
    try {
      await engine.setGizmoCoordinateMode(newMode);
      console.log(`[Toolbar] Coordinate mode set to ${newMode}`);
    } catch (error) {
      console.error('[Toolbar] Failed to set coordinate mode:', error);
    }
  };

  const handleCreateObject = async (type: string) => {
    try {
      console.log(`[Toolbar] Creating ${type}...`);
      
      if (!engine.createPrimitive) {
        console.error('[Toolbar] engine.createPrimitive is not available');
        return;
      }
      
      // 使用 Undo 系统创建图元
      const command = new CreatePrimitiveCommand(type);
      const undoManager = getUndoManager();
      
      // 手动执行并推入栈
      await command.execute();
      undoManager.pushExecutedCommand(command);
      
      setShowCreateMenu(false);
      console.log(`[Toolbar] Successfully created ${type} with Undo support`);
    } catch (error) {
      console.error(`[Toolbar] Failed to create ${type}:`, error);
    }
  };

  // 点击外部关闭菜单
  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(event.target as Node)) {
        setShowCreateMenu(false);
      }
      if (csgMenuRef.current && !csgMenuRef.current.contains(event.target as Node)) {
        setShowCSGMenu(false);
      }
    };

    if (showCreateMenu || showCSGMenu) {
      document.addEventListener('mousedown', handleClickOutside);
    }

    return () => {
      document.removeEventListener('mousedown', handleClickOutside);
    };
  }, [showCreateMenu, showCSGMenu]);
  
  const handleCSGPrimitive = async (type: 'box' | 'sphere' | 'cylinder' | 'cone') => {
    try {
      if (!engine.createPrimitive) {
        console.error('[Toolbar] engine.createPrimitive is not available');
        return;
      }
      
      // CSG几何体使用 'csg_' 前缀来区分普通几何体
      await engine.createPrimitive(`csg_${type}`);
      setShowCSGMenu(false);
      console.log(`[Toolbar] Created CSG primitive: ${type}`);
    } catch (error) {
      console.error(`[Toolbar] Failed to create CSG primitive:`, error);
    }
  };
  
  const handleCSGBoolean = async (operation: 'union' | 'subtract' | 'intersect') => {
    // TODO: 需要实现多选功能才能进行布尔运算
    console.warn('[Toolbar] Boolean operations require multi-selection (not yet implemented)');
    setShowCSGMenu(false);
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

      {/* 🎯 World/Local 坐标系切换（Unity 风格） */}
      <div className={styles.section}>
        <button
          className={`${styles.button} ${styles.coordinateToggle}`}
          onClick={handleCoordinateModeToggle}
          title={`Coordinate System: ${coordinateMode === 'world' ? 'World' : 'Local'}`}
        >
          {coordinateMode === 'world' ? '🌍 World' : '📍 Local'}
        </button>
      </div>
      
      <div className={styles.section} ref={menuRef}>
        <span className={styles.label}>Create:</span>
        <button 
          className={styles.button} 
          onClick={() => setShowCreateMenu(!showCreateMenu)}
          title="Create Object"
        >
          + Add
        </button>
        
        {showCreateMenu && (
          <div className={styles.dropdown}>
            <div className={styles.dropdownLabel}>基础对象</div>
            <button onClick={() => handleCreateObject('empty')} className={styles.dropdownItem}>
              📦 Empty Node
            </button>
            <button onClick={() => handleCreateObject('cube')} className={styles.dropdownItem}>
              ⬜ Cube
            </button>
            <button onClick={() => handleCreateObject('sphere')} className={styles.dropdownItem}>
              ⚫ Sphere
            </button>
            <button onClick={() => handleCreateObject('cylinder')} className={styles.dropdownItem}>
              🔵 Cylinder
            </button>
            <button onClick={() => handleCreateObject('plane')} className={styles.dropdownItem}>
              ▭ Plane
            </button>
            <div className={styles.dropdownDivider}></div>
            <div className={styles.dropdownLabel}>场景元素</div>
            <button onClick={() => handleCreateObject('light')} className={styles.dropdownItem}>
              💡 Light
            </button>
            <button onClick={() => handleCreateObject('skybox')} className={styles.dropdownItem}>
              🌌 Skybox
            </button>
          </div>
        )}
      </div>
      
      {/* CSG工具 */}
      <div className={styles.section} ref={csgMenuRef}>
        <span className={styles.label}>CSG:</span>
        <button 
          className={styles.button} 
          onClick={() => setShowCSGMenu(!showCSGMenu)}
          title="CSG Tools"
        >
          ⚙ CSG
        </button>
        
        {showCSGMenu && (
          <div className={styles.dropdown}>
            <div className={styles.dropdownLabel}>Primitives</div>
            <button onClick={() => handleCSGPrimitive('box')} className={styles.dropdownItem}>
              📦 Box
            </button>
            <button onClick={() => handleCSGPrimitive('sphere')} className={styles.dropdownItem}>
              ⚪ Sphere
            </button>
            <button onClick={() => handleCSGPrimitive('cylinder')} className={styles.dropdownItem}>
              ⚙️ Cylinder
            </button>
            <button onClick={() => handleCSGPrimitive('cone')} className={styles.dropdownItem}>
              🔺 Cone
            </button>
            <div className={styles.dropdownDivider}></div>
            <div className={styles.dropdownLabel}>Boolean (需要多选)</div>
            <button 
              onClick={() => handleCSGBoolean('union')} 
              className={styles.dropdownItem}
              disabled
              title="Select 2+ objects first"
            >
              ➕ Union
            </button>
            <button 
              onClick={() => handleCSGBoolean('subtract')} 
              className={styles.dropdownItem}
              disabled
              title="Select 2+ objects first"
            >
              ➖ Subtract
            </button>
            <button 
              onClick={() => handleCSGBoolean('intersect')} 
              className={styles.dropdownItem}
              disabled
              title="Select 2+ objects first"
            >
              ✂️ Intersect
            </button>
          </div>
        )}
      </div>
    </div>
  );
};

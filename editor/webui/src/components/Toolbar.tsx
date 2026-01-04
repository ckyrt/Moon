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
  const [coordinateMode, setCoordinateMode] = useState<'world' | 'local'>('world');  // 🎯 World/Local 模式
  const menuRef = useRef<HTMLDivElement>(null);

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
    };

    if (showCreateMenu) {
      document.addEventListener('mousedown', handleClickOutside);
    }

    return () => {
      document.removeEventListener('mousedown', handleClickOutside);
    };
  }, [showCreateMenu]);

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
            <button onClick={() => handleCreateObject('light')} className={styles.dropdownItem}>
              💡 Light
            </button>
            <button onClick={() => handleCreateObject('skybox')} className={styles.dropdownItem}>
              🌌 Skybox
            </button>
          </div>
        )}
      </div>
    </div>
  );
};

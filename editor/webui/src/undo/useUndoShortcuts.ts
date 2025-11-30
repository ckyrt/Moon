/**
 * Moon Engine - Undo Keyboard Shortcuts Hook
 * 提供 Ctrl+Z (Undo) 和 Ctrl+Y (Redo) 快捷键支持
 */

import { useEffect } from 'react';
import { getUndoManager } from '@/undo';

/**
 * useUndoShortcuts - 注册 Undo/Redo 键盘快捷键
 * 
 * 快捷键：
 * - Ctrl+Z: Undo
 * - Ctrl+Y: Redo
 * - Ctrl+Shift+Z: Redo (可选)
 */
export function useUndoShortcuts() {
  useEffect(() => {
    const undoManager = getUndoManager();

    const handleKeyDown = async (event: KeyboardEvent) => {
      // 检查是否按下 Ctrl (Windows/Linux) 或 Cmd (Mac)
      const isCtrlOrCmd = event.ctrlKey || event.metaKey;

      if (!isCtrlOrCmd) return;

      // Ctrl+Z: Undo
      if (event.key === 'z' && !event.shiftKey) {
        event.preventDefault();
        const success = await undoManager.undo();
        if (success) {
          console.log('[Shortcuts] Undo executed');
        }
        return;
      }

      // Ctrl+Shift+Z: Redo
      if (event.key === 'z' && event.shiftKey) {
        event.preventDefault();
        const success = await undoManager.redo();
        if (success) {
          console.log('[Shortcuts] Redo executed (Ctrl+Shift+Z)');
        }
        return;
      }

      // Ctrl+Y: Redo
      if (event.key === 'y') {
        event.preventDefault();
        const success = await undoManager.redo();
        if (success) {
          console.log('[Shortcuts] Redo executed (Ctrl+Y)');
        }
        return;
      }
    };

    // 注册全局键盘事件
    window.addEventListener('keydown', handleKeyDown);

    // 清理
    return () => {
      window.removeEventListener('keydown', handleKeyDown);
    };
  }, []);
}

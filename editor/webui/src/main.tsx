import { createRoot } from 'react-dom/client'
import { App } from './ui/App'
import { runSimpleHierarchyTest } from './tests/simpleHierarchyTest'
import { runNodePropertiesTest } from './tests/nodePropertiesTest'
import { getUndoManager } from './undo/UndoManager'

const root = document.getElementById('root')!
createRoot(root).render(<App />)

// 导出测试函数到全局对象（用于开发者工具调用）
declare global {
  interface Window {
    runSimpleHierarchyTest: () => Promise<void>;
    runNodePropertiesTest: () => Promise<void>;
    debugUndoManager: () => void;
  }
}

window.runSimpleHierarchyTest = runSimpleHierarchyTest;
window.runNodePropertiesTest = runNodePropertiesTest;

// 🔍 调试函数：打印 UndoManager 状态
window.debugUndoManager = () => {
  const undoManager = getUndoManager();
  const undoStack = (undoManager as any)['undoStack'] || [];
  const redoStack = (undoManager as any)['redoStack'] || [];
  
  console.log('\n🔍 === UndoManager 状态 ===');
  console.log(`Undo Stack (${undoStack.length}):`);
  undoStack.forEach((cmd: any, i: number) => {
    console.log(`  ${i + 1}. ${cmd.description || cmd.constructor.name}`);
  });
  
  console.log(`Redo Stack (${redoStack.length}):`);
  redoStack.forEach((cmd: any, i: number) => {
    console.log(`  ${i + 1}. ${cmd.description || cmd.constructor.name}`);
  });
  
  console.log(`\nCan Undo: ${undoManager.canUndo()}`);
  console.log(`Can Redo: ${undoManager.canRedo()}`);
};

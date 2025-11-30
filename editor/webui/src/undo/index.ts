/**
 * Moon Engine - Undo System
 * 统一导出所有 Undo/Redo 相关类和函数
 */

export type { Command } from './Command';
export { MultiCommand } from './Command';
export { UndoManager, getUndoManager, resetUndoManager } from './UndoManager';
export type { UndoManagerOptions } from './UndoManager';

export {
  SetPositionCommand,
  SetRotationCommand,
  SetScaleCommand,
  SetTransformCommand,
} from './TransformCommands';

export {
  CreateNodeCommand,
  DeleteNodeCommand,
  RenameNodeCommand,
  SetNodeActiveCommand,
  SetParentCommand,
} from './NodeCommands';

export { CreatePrimitiveCommand } from './PrimitiveCommands';

export { useUndoShortcuts } from './useUndoShortcuts';
export { onGizmoStart, onGizmoEnd } from './gizmoUndo';

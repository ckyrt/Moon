/**
 * Undo Manager - Command stack manager
 */

import type { Command } from './Command';
import { logger } from '@/utils/logger';

export interface UndoManagerOptions {
  maxHistorySize?: number;
  debug?: boolean;
}

export class UndoManager {
  private undoStack: Command[] = [];
  private redoStack: Command[] = [];
  private maxHistorySize: number;
  private debug: boolean;
  private isExecuting = false;

  constructor(options: UndoManagerOptions = {}) {
    this.maxHistorySize = options.maxHistorySize ?? 50;
    this.debug = options.debug ?? false;
  }

  execute(command: Command): void {
    if (this.isExecuting) {
      logger.warn('UndoManager', 'Recursive execute() detected, ignoring');
      return;
    }

    this.isExecuting = true;

    try {
      command.execute();
      this.undoStack.push(command);

      if (this.undoStack.length > this.maxHistorySize) {
        this.undoStack.shift();
      }

      this.redoStack = [];

      logger.info('UndoManager', `Executed: ${command.description || command.constructor.name}`, {
        undoCount: this.undoStack.length
      });

      if (this.debug) {
        console.log('[UndoManager] Executed:', command.description || command.constructor.name);
        this.printStatus();
      }
    } finally {
      this.isExecuting = false;
    }
  }

  /**
   * Push already-executed command (e.g., Gizmo drag)
   */
  pushExecutedCommand(command: Command): void {
    this.undoStack.push(command);

    if (this.undoStack.length > this.maxHistorySize) {
      this.undoStack.shift();
    }

    this.redoStack = [];

    if (this.debug) {
      console.log('[UndoManager] Pushed executed command:', command.description || command.constructor.name);
      this.printStatus();
    }
  }

  async undo(): Promise<boolean> {
    if (this.undoStack.length === 0) {
      if (this.debug) console.log('[UndoManager] Nothing to undo');
      return false;
    }

    const command = this.undoStack.pop()!;

    try {
      await command.undo();
      this.redoStack.push(command);

      logger.info('UndoManager', `Undo: ${command.description || command.constructor.name}`);

      if (this.debug) {
        console.log('[UndoManager] Undone:', command.description || command.constructor.name);
        this.printStatus();
      }

      return true;
    } catch (error) {
      logger.error('UndoManager', `Undo failed: ${command.description || command.constructor.name}`, { error });
      this.undoStack.push(command);
      return false;
    }
  }

  async redo(): Promise<boolean> {
    if (this.redoStack.length === 0) {
      if (this.debug) console.log('[UndoManager] Nothing to redo');
      return false;
    }

    const command = this.redoStack.pop()!;

    try {
      await command.execute();
      this.undoStack.push(command);

      logger.info('UndoManager', `Redo: ${command.description || command.constructor.name}`);

      if (this.debug) {
        console.log('[UndoManager] Redone:', command.description || command.constructor.name);
        this.printStatus();
      }

      return true;
    } catch (error) {
      logger.error('UndoManager', `Redo failed: ${command.description || command.constructor.name}`, { error });
      this.redoStack.push(command);
      return false;
    }
  }

  clear(): void {
    this.undoStack = [];
    this.redoStack = [];
    if (this.debug) console.log('[UndoManager] Cleared all history');
  }

  canUndo(): boolean {
    return this.undoStack.length > 0;
  }

  canRedo(): boolean {
    return this.redoStack.length > 0;
  }

  get undoCount(): number {
    return this.undoStack.length;
  }

  get redoCount(): number {
    return this.redoStack.length;
  }

  getUndoDescription(): string | undefined {
    const command = this.undoStack[this.undoStack.length - 1];
    return command?.description || command?.constructor.name;
  }

  getRedoDescription(): string | undefined {
    const command = this.redoStack[this.redoStack.length - 1];
    return command?.description || command?.constructor.name;
  }

  private printStatus(): void {
    console.log(`[UndoManager] Undo: ${this.undoCount}, Redo: ${this.redoCount}`);
  }
}

let globalUndoManager: UndoManager | null = null;

export function getUndoManager(options?: UndoManagerOptions): UndoManager {
  if (!globalUndoManager) {
    globalUndoManager = new UndoManager(options);
  }
  return globalUndoManager;
}

export function resetUndoManager(): void {
  globalUndoManager = null;
}

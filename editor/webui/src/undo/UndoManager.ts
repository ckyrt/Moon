/**
 * Moon Engine - Undo Manager
 * 管理命令栈，提供 Undo/Redo 功能
 */

import type { Command } from './Command';
import { logger } from '@/utils/logger';

/**
 * UndoManager 配置选项
 */
export interface UndoManagerOptions {
  /**
   * 最大历史记录数量（默认 50）
   */
  maxHistorySize?: number;

  /**
   * 是否启用调试日志（默认 false）
   */
  debug?: boolean;
}

/**
 * UndoManager - Undo/Redo 系统核心
 * 
 * 职责：
 * - 管理 undoStack 和 redoStack
 * - 执行命令并入栈
 * - 提供 undo/redo 操作
 * - 限制历史记录大小
 * 
 * 特性：
 * - 独立模块，不依赖 Scene、Renderer、UI
 * - 只存储 Command 对象
 * - 支持批量操作（MultiCommand）
 */
export class UndoManager {
  private undoStack: Command[] = [];
  private redoStack: Command[] = [];
  private maxHistorySize: number;
  private debug: boolean;
  private isExecuting = false; // 防止递归执行

  constructor(options: UndoManagerOptions = {}) {
    this.maxHistorySize = options.maxHistorySize ?? 50;
    this.debug = options.debug ?? false;
  }

  /**
   * 执行命令并加入 Undo 栈
   * 
   * @param command 要执行的命令
   */
  execute(command: Command): void {
    if (this.isExecuting) {
      console.warn('[UndoManager] Recursive execute() detected, ignoring');
      logger.warn('UndoManager', 'Recursive execute() detected, ignoring');
      return;
    }

    this.isExecuting = true;

    try {
      // 1. 执行命令
      command.execute();

      // 2. 加入 undo 栈
      this.undoStack.push(command);

      // 3. 限制栈大小
      if (this.undoStack.length > this.maxHistorySize) {
        this.undoStack.shift(); // 移除最旧的命令
      }

      // 4. 清空 redo 栈（执行新命令后不能再 redo）
      this.redoStack = [];

      logger.info('UndoManager', `Executed: ${command.description || command.constructor.name}`, {
        undoCount: this.undoStack.length,
        redoCount: this.redoStack.length
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
   * 将已执行的命令推入 Undo 栈（不执行）
   * 
   * 用于 Gizmo 拖拽等场景：操作已经完成，只需要记录以便 Undo
   * 
   * @param command 已执行的命令
   */
  pushExecutedCommand(command: Command): void {
    // 加入 undo 栈
    this.undoStack.push(command);

    // 限制栈大小
    if (this.undoStack.length > this.maxHistorySize) {
      this.undoStack.shift();
    }

    // 清空 redo 栈
    this.redoStack = [];

    if (this.debug) {
      console.log('[UndoManager] Pushed executed command:', command.description || command.constructor.name);
      this.printStatus();
    }
  }

  /**
   * 撤销上一个命令
   * 
   * @returns 是否成功撤销
   */
  async undo(): Promise<boolean> {
    if (this.undoStack.length === 0) {
      logger.warn('UndoManager', 'Nothing to undo - stack is empty');
      if (this.debug) console.log('[UndoManager] Nothing to undo');
      return false;
    }

    const command = this.undoStack.pop()!;

    try {
      logger.info('UndoManager', `Undoing: ${command.description || command.constructor.name}`, {
        remainingUndoCount: this.undoStack.length,
        redoCount: this.redoStack.length
      });

      // 1. 撤销命令（支持异步）
      await command.undo();

      // 2. 加入 redo 栈
      this.redoStack.push(command);

      logger.info('UndoManager', `Undo complete: ${command.description || command.constructor.name}`, {
        undoCount: this.undoStack.length,
        redoCount: this.redoStack.length
      });

      if (this.debug) {
        console.log('[UndoManager] Undone:', command.description || command.constructor.name);
        this.printStatus();
      }

      return true;
    } catch (error) {
      logger.error('UndoManager', `Undo failed: ${command.description || command.constructor.name}`, { error });
      console.error('[UndoManager] Undo failed:', error);
      // 撤销失败，恢复到 undo 栈
      this.undoStack.push(command);
      return false;
    }
  }

  /**
   * 重做上一个撤销的命令
   * 
   * @returns 是否成功重做
   */
  async redo(): Promise<boolean> {
    if (this.redoStack.length === 0) {
      logger.warn('UndoManager', 'Nothing to redo - stack is empty');
      if (this.debug) console.log('[UndoManager] Nothing to redo');
      return false;
    }

    const command = this.redoStack.pop()!;

    try {
      logger.info('UndoManager', `Redoing: ${command.description || command.constructor.name}`, {
        undoCount: this.undoStack.length,
        remainingRedoCount: this.redoStack.length
      });

      // 1. 重新执行命令（支持异步）
      await command.execute();

      // 2. 加入 undo 栈
      this.undoStack.push(command);

      logger.info('UndoManager', `Redo complete: ${command.description || command.constructor.name}`, {
        undoCount: this.undoStack.length,
        redoCount: this.redoStack.length
      });

      if (this.debug) {
        console.log('[UndoManager] Redone:', command.description || command.constructor.name);
        this.printStatus();
      }

      return true;
    } catch (error) {
      logger.error('UndoManager', `Redo failed: ${command.description || command.constructor.name}`, { error });
      console.error('[UndoManager] Redo failed:', error);
      // 重做失败，恢复到 redo 栈
      this.redoStack.push(command);
      return false;
    }
  }

  /**
   * 清空所有历史记录
   */
  clear(): void {
    this.undoStack = [];
    this.redoStack = [];

    if (this.debug) {
      console.log('[UndoManager] Cleared all history');
    }
  }

  /**
   * 检查是否可以撤销
   */
  canUndo(): boolean {
    return this.undoStack.length > 0;
  }

  /**
   * 检查是否可以重做
   */
  canRedo(): boolean {
    return this.redoStack.length > 0;
  }

  /**
   * 获取 Undo 栈大小
   */
  get undoCount(): number {
    return this.undoStack.length;
  }

  /**
   * 获取 Redo 栈大小
   */
  get redoCount(): number {
    return this.redoStack.length;
  }

  /**
   * 获取最近的命令描述（用于 UI 显示）
   */
  getUndoDescription(): string | undefined {
    const command = this.undoStack[this.undoStack.length - 1];
    return command?.description || command?.constructor.name;
  }

  getRedoDescription(): string | undefined {
    const command = this.redoStack[this.redoStack.length - 1];
    return command?.description || command?.constructor.name;
  }

  /**
   * 打印当前状态（调试用）
   */
  private printStatus(): void {
    console.log(`[UndoManager] Undo: ${this.undoCount}, Redo: ${this.redoCount}`);
  }
}

// 全局单例
let globalUndoManager: UndoManager | null = null;

/**
 * 获取全局 UndoManager 单例
 */
export function getUndoManager(options?: UndoManagerOptions): UndoManager {
  if (!globalUndoManager) {
    globalUndoManager = new UndoManager(options);
  }
  return globalUndoManager;
}

/**
 * 重置全局 UndoManager（主要用于测试）
 */
export function resetUndoManager(): void {
  globalUndoManager = null;
}

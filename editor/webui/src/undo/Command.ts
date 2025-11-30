/**
 * Moon Engine - Undo/Redo System
 * Command Pattern Implementation
 */

/**
 * Command 接口 - 所有可撤销操作的基类
 * 
 * 每个命令必须实现：
 * - execute(): 执行操作（前进）
 * - undo(): 撤销操作（后退）
 * 
 * 🎯 设计原则（参考 Unity/Unreal）：
 * 1. 命令创建时捕获所有状态（不在 execute 时）
 * 2. Execute 和 Redo 行为完全一致（幂等）
 * 3. 使用完整快照而非增量修改
 */
export interface Command {
  /**
   * 执行命令（前进操作）
   * 🎯 Execute 和 Redo 必须完全一致
   */
  execute(): void | Promise<void>;

  /**
   * 撤销命令（恢复到执行前的状态）
   */
  undo(): void | Promise<void>;

  /**
   * 命令描述（可选，用于调试）
   */
  description?: string;
}

/**
 * MultiCommand - 批量命令包装器
 * 将多个命令组合成一个可撤销单元
 * 
 * 使用场景：
 * - 同时修改 Position 的 X、Y、Z
 * - 复制粘贴（创建节点 + 设置属性）
 * - 拖拽多个节点
 */
export class MultiCommand implements Command {
  private commands: Command[] = [];
  description?: string;

  constructor(commands: Command[], description?: string) {
    this.commands = commands;
    this.description = description;
  }

  execute(): void {
    // 顺序执行所有命令
    for (const command of this.commands) {
      command.execute();
    }
  }

  undo(): void {
    // 逆序撤销所有命令
    for (let i = this.commands.length - 1; i >= 0; i--) {
      this.commands[i].undo();
    }
  }

  /**
   * 添加子命令
   */
  addCommand(command: Command): void {
    this.commands.push(command);
  }

  /**
   * 获取子命令数量
   */
  get commandCount(): number {
    return this.commands.length;
  }
}

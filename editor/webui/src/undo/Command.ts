/**
 * Command interface - Base for all undoable operations
 * 
 * Design principles:
 * 1. Capture state at construction, not in execute()
 * 2. execute() = redo() (idempotent)
 * 3. Commands are independent and self-contained
 */
export interface Command {
  execute(): void | Promise<void>;
  undo(): void | Promise<void>;
  description?: string;
}

/**
 * Batch multiple commands into a single undo unit
 */
export class MultiCommand implements Command {
  private commands: Command[] = [];
  description?: string;

  constructor(commands: Command[], description?: string) {
    this.commands = commands;
    this.description = description;
  }

  execute(): void {
    for (const command of this.commands) {
      command.execute();
    }
  }

  undo(): void {
    for (let i = this.commands.length - 1; i >= 0; i--) {
      this.commands[i].undo();
    }
  }

  addCommand(command: Command): void {
    this.commands.push(command);
  }

  get commandCount(): number {
    return this.commands.length;
  }
}

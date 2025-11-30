/**
 * 前端日志系统
 * 通过 CEF Bridge 将日志写入到 C++ 端的文件
 */

interface LogEntry {
  timestamp: string;
  level: 'INFO' | 'WARN' | 'ERROR' | 'DEBUG';
  category: string;
  message: string;
  data?: any;
}

class Logger {
  private logBuffer: LogEntry[] = [];
  private flushInterval: number = 1000; // 每秒刷新一次
  private timer: number | null = null;

  constructor() {
    this.startAutoFlush();
  }

  private startAutoFlush() {
    this.timer = window.setInterval(() => {
      this.flush();
    }, this.flushInterval);
  }

  private formatTimestamp(): string {
    const now = new Date();
    return now.toISOString().replace('T', ' ').substring(0, 23);
  }

  info(category: string, message: string, data?: any) {
    this.log('INFO', category, message, data);
  }

  warn(category: string, message: string, data?: any) {
    this.log('WARN', category, message, data);
  }

  error(category: string, message: string, data?: any) {
    this.log('ERROR', category, message, data);
  }

  debug(category: string, message: string, data?: any) {
    this.log('DEBUG', category, message, data);
  }

  private log(level: LogEntry['level'], category: string, message: string, data?: any) {
    const entry: LogEntry = {
      timestamp: this.formatTimestamp(),
      level,
      category,
      message,
      data
    };

    this.logBuffer.push(entry);
    
    // 同时输出到 console
    const consoleMsg = `[${entry.timestamp}] [${level}] [${category}] ${message}`;
    if (data !== undefined) {
      console.log(consoleMsg, data);
    } else {
      console.log(consoleMsg);
    }

    // 立即刷新重要日志
    if (level === 'ERROR') {
      this.flush();
    }
  }

  private flush() {
    if (this.logBuffer.length === 0) return;

    const logs = [...this.logBuffer];
    this.logBuffer = [];

    // 通过 CEF 发送到 C++ 端
    if (window.cefQuery) {
      const logContent = logs.map(entry => {
        let line = `[${entry.timestamp}] [${entry.level}] [${entry.category}] ${entry.message}`;
        if (entry.data !== undefined) {
          try {
            line += ` | Data: ${JSON.stringify(entry.data)}`;
          } catch (e) {
            line += ` | Data: [Circular or non-serializable]`;
          }
        }
        return line;
      }).join('\n');

      window.cefQuery({
        request: JSON.stringify({
          command: 'writeLog',
          logContent: logContent + '\n'
        }),
        onSuccess: () => {
          // Log written successfully
        },
        onFailure: (_errorCode: number, errorMessage: string) => {
          console.error('[Logger] Failed to write log:', errorMessage);
        }
      });
    }
  }

  destroy() {
    if (this.timer !== null) {
      clearInterval(this.timer);
      this.timer = null;
    }
    this.flush(); // 最后刷新一次
  }
}

// 全局单例
export const logger = new Logger();

// 清理函数
window.addEventListener('beforeunload', () => {
  logger.destroy();
});

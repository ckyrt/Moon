/**
 * Moon Editor - 3D Viewport Component
 */

import React, { useEffect, useRef } from 'react';
import { isConnectedToEngine } from '@/utils/engine-bridge';
import styles from './Viewport.module.css';

export const Viewport: React.FC = () => {
  const canvasRef = useRef<HTMLDivElement>(null);
  const [isConnected, setIsConnected] = React.useState(false);

  useEffect(() => {
    setIsConnected(isConnectedToEngine());

    // ✅ 工业级方案 A：将 viewport div 的屏幕坐标发送给 C++
    const updateViewportBounds = () => {
      if (!canvasRef.current) return;
      
      // 获取 div 相对于浏览器窗口的精确位置
      const rect = canvasRef.current.getBoundingClientRect();
      
      // ✅ 重要：getBoundingClientRect() 是相对于视口（viewport）的坐标
      // 我们需要相对于整个页面（包括滚动）的坐标
      // 但是在我们的场景中，页面没有滚动，所以可以直接使用 rect.left/top
      // 如果页面有滚动，需要加上 window.scrollX/scrollY
      const x = Math.round(rect.left + window.scrollX);
      const y = Math.round(rect.top + window.scrollY);
      
      console.log('[Viewport] Sending rect to C++:', {
        left: rect.left,
        top: rect.top,
        scrollX: window.scrollX,
        scrollY: window.scrollY,
        finalX: x,
        finalY: y,
        width: rect.width,
        height: rect.height,
        hasCefQuery: !!window.cefQuery,
        hasMoonEngine: !!window.moonEngine
      });
      
      // 方法 1: 使用 CEF Query (推荐，CEF 标准方式)
      if (window.cefQuery) {
        console.log('[Viewport] Using cefQuery');
        window.cefQuery({
          request: JSON.stringify({
            type: 'viewport-rect',
            x: x,
            y: y,
            width: Math.round(rect.width),
            height: Math.round(rect.height)
          }),
          onSuccess: (response) => {
            console.log('[Viewport] cefQuery success:', response);
          },
          onFailure: (errorCode, errorMessage) => {
            console.error('[Viewport] cefQuery failed:', errorCode, errorMessage);
          }
        });
      }
      // 方法 2: 使用自定义 moonEngine API (备用)
      else if (window.moonEngine?.setViewportBounds) {
        console.log('[Viewport] Using moonEngine.setViewportBounds');
        window.moonEngine.setViewportBounds(
          x,
          y,
          Math.round(rect.width),
          Math.round(rect.height)
        );
      }
      else {
        // 开发模式：输出到控制台
        console.warn('[Viewport] No C++ bridge available, running in mock mode');
        console.log('[Viewport] Rect:', rect);
      }
    };

    // 初始化时延迟更新（等待布局稳定）
    const timer = setTimeout(updateViewportBounds, 200);

    // 监听容器大小变化（拖拽、窗口调整等）
    const resizeObserver = new ResizeObserver(() => {
      // 防抖：避免过于频繁更新
      setTimeout(updateViewportBounds, 16); // ~60fps
    });
    
    if (canvasRef.current) {
      resizeObserver.observe(canvasRef.current);
    }

    // 监听窗口大小变化（CEF 窗口调整）
    window.addEventListener('resize', updateViewportBounds);

    return () => {
      clearTimeout(timer);
      resizeObserver.disconnect();
      window.removeEventListener('resize', updateViewportBounds);
    };
  }, []);

  return (
    <div className={styles.viewport}>
      <div className={styles.header}>
        <span className={styles.title}>Viewport</span>
        <div className={styles.status}>
          <span className={`${styles.indicator} ${isConnected ? styles.connected : ''}`} />
          <span className={styles.statusText}>
            {isConnected ? 'Connected to Engine' : 'Mock Mode (Development)'}
          </span>
        </div>
      </div>
      <div ref={canvasRef} className={styles.canvas} id="viewport-container">
        {!isConnected && (
          <div className={styles.placeholder}>
            <div className={styles.placeholderContent}>
              <div className={styles.placeholderIcon}>📦</div>
              <div className={styles.placeholderText}>
                3D Viewport
                <br />
                <small>(CEF Native Rendering will be embedded here)</small>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

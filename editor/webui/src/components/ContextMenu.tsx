/**
 * Moon Editor - Context Menu Component
 */

import React, { useEffect, useRef } from 'react';
import styles from './ContextMenu.module.css';

export interface ContextMenuItem {
  label: string;
  onClick: () => void;
  disabled?: boolean;
  separator?: boolean;
}

export interface ContextMenuProps {
  items: ContextMenuItem[];
  position: { x: number; y: number };
  onClose: () => void;
}

export const ContextMenu: React.FC<ContextMenuProps> = ({ items, position, onClose }) => {
  const menuRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(event.target as Node)) {
        onClose();
      }
    };

    const handleEscape = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        onClose();
      }
    };

    document.addEventListener('mousedown', handleClickOutside);
    document.addEventListener('keydown', handleEscape);

    return () => {
      document.removeEventListener('mousedown', handleClickOutside);
      document.removeEventListener('keydown', handleEscape);
    };
  }, [onClose]);

  return (
    <div
      ref={menuRef}
      className={styles.contextMenu}
      style={{
        left: `${position.x}px`,
        top: `${position.y}px`,
      }}
    >
      {items.map((item, index) =>
        item.separator ? (
          <div key={index} className={styles.separator} />
        ) : (
          <div
            key={index}
            className={`${styles.menuItem} ${item.disabled ? styles.disabled : ''}`}
            onClick={() => {
              if (!item.disabled) {
                item.onClick();
                onClose();
              }
            }}
          >
            {item.label}
          </div>
        )
      )}
    </div>
  );
};

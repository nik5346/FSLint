import React, { useState, useLayoutEffect, useRef, ReactNode, useMemo } from 'react';

interface VirtualListProps<T> {
  items: T[];
  itemHeight: number;
  renderItem: (item: T, index: number) => ReactNode;
  containerStyle?: React.CSSProperties;
  overscan?: number;
}

export function VirtualList<T>({
  items,
  itemHeight,
  renderItem,
  containerStyle,
  overscan = 40,
}: VirtualListProps<T>) {
  const containerRef = useRef<HTMLDivElement>(null);
  const [scrollTop, setScrollTop] = useState(0);
  const [containerHeight, setContainerHeight] = useState(0);

  useLayoutEffect(() => {
    const container = containerRef.current;
    if (!container) return;

    const handleScroll = () => {
      setScrollTop(container.scrollTop);
    };

    const resizeObserver = new ResizeObserver((entries) => {
      for (const entry of entries) {
        if (entry.contentRect.height > 0) {
          setContainerHeight(entry.contentRect.height);
        }
      }
    });

    const initialHeight = container.clientHeight;
    if (initialHeight > 0) {
      setContainerHeight(initialHeight);
    }

    container.addEventListener('scroll', handleScroll, { passive: true });
    resizeObserver.observe(container);

    return () => {
      container.removeEventListener('scroll', handleScroll);
      resizeObserver.disconnect();
    };
  }, []);

  const totalHeight = items.length * itemHeight;

  const visibleIndices = useMemo(() => {
    const startIndex = Math.max(0, Math.floor(scrollTop / itemHeight) - overscan);
    const endIndex = Math.min(
      items.length - 1,
      Math.ceil((scrollTop + containerHeight) / itemHeight) + overscan,
    );
    return { startIndex, endIndex };
  }, [scrollTop, containerHeight, itemHeight, items.length, overscan]);

  const visibleItems = useMemo(() => {
    const { startIndex, endIndex } = visibleIndices;
    const itemsToRender = [];
    for (let i = startIndex; i <= endIndex; i++) {
      itemsToRender.push(
        <div
          key={i}
          style={{
            position: 'absolute',
            top: i * itemHeight,
            left: 0,
            minWidth: '100%',
            width: 'fit-content',
            height: itemHeight,
          }}
        >
          {renderItem(items[i], i)}
        </div>,
      );
    }
    return itemsToRender;
  }, [visibleIndices, itemHeight, items, renderItem]);

  return (
    <div
      ref={containerRef}
      style={{
        ...containerStyle,
        position: 'relative',
        overflow: 'auto',
        height: '100%',
        WebkitOverflowScrolling: 'touch',
      }}
    >
      <div
        style={{
          height: totalHeight,
          minWidth: '100%',
          width: 'fit-content',
          position: 'relative',
          willChange: 'transform',
        }}
      >
        {visibleItems}
      </div>
    </div>
  );
}

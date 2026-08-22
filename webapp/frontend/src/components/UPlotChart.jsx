import React, { useEffect, useRef } from 'react';
import uPlot from 'uplot';
import 'uplot/dist/uPlot.min.css';

export default function UPlotChart({ data, options, className = '', height }) {
  const containerRef = useRef(null);
  const chartInstanceRef = useRef(null);

  useEffect(() => {
    if (!containerRef.current || !data || data.length < 2 || !data[0] || data[0].length === 0) {
      if (chartInstanceRef.current) {
        chartInstanceRef.current.destroy();
        chartInstanceRef.current = null;
      }
      return;
    }

    const rect = containerRef.current.getBoundingClientRect();
    const width = Math.max(100, Math.floor(rect.width) || 400);
    const chartHeight = height || Math.max(150, Math.floor(rect.height) || 240);

    const defaultOptions = {
      width,
      height: chartHeight,
      cursor: {
        drag: { setScale: true },
        sync: { key: 'shm_sync' },
        points: {
          size: (u, seriesIdx) => 6,
          width: (u, seriesIdx, size) => 2,
          stroke: (u, seriesIdx) => '#10b981',
          fill: (u, seriesIdx) => '#0f172a',
        },
      },
      legend: {
        show: true,
        live: false,
      },
      scales: {
        x: { time: true },
        y: { auto: true },
      },
      axes: [
        {
          stroke: '#94a3b8',
          grid: { stroke: '#1e293b', width: 1 },
          ticks: { stroke: '#334155', width: 1, size: 4 },
          font: '10px Inter, system-ui, sans-serif',
        },
        {
          stroke: '#94a3b8',
          grid: { stroke: '#1e293b', width: 1 },
          ticks: { stroke: '#334155', width: 1, size: 4 },
          font: '10px Inter, system-ui, sans-serif',
        },
      ],
      series: [{}],
    };

    // Mergear opciones
    const mergedOptions = {
      ...defaultOptions,
      ...options,
      width,
      height: chartHeight,
      axes: options?.axes || defaultOptions.axes,
      scales: options?.scales || defaultOptions.scales,
      series: options?.series || defaultOptions.series,
      cursor: { ...defaultOptions.cursor, ...options?.cursor },
    };

    if (chartInstanceRef.current) {
      chartInstanceRef.current.destroy();
    }

    try {
      const u = new uPlot(mergedOptions, data, containerRef.current);
      chartInstanceRef.current = u;

      // Doble click para restaurar zoom
      const handleDblClick = () => {
        if (data && data[0] && data[0].length > 0) {
          u.setData(data, true);
        }
      };

      const dom = u.root;
      dom.addEventListener('dblclick', handleDblClick);

      return () => {
        dom.removeEventListener('dblclick', handleDblClick);
        u.destroy();
        chartInstanceRef.current = null;
      };
    } catch (err) {
      console.error('Error instanciando uPlot:', err);
    }
  }, [data, options, height]);

  // Manejo de ResizeObserver para responder a cambios de pantalla fluidamente
  useEffect(() => {
    if (!containerRef.current) return;

    const ro = new ResizeObserver((entries) => {
      for (const entry of entries) {
        if (chartInstanceRef.current && entry.contentRect.width > 0) {
          const newWidth = Math.floor(entry.contentRect.width);
          const newHeight = height || Math.floor(entry.contentRect.height) || chartInstanceRef.current.height;
          chartInstanceRef.current.setSize({ width: newWidth, height: newHeight });
        }
      }
    });

    ro.observe(containerRef.current);
    return () => ro.disconnect();
  }, [height]);

  if (!data || data.length < 2 || !data[0] || data[0].length === 0) {
    return (
      <div className="h-full w-full flex items-center justify-center text-xs text-slate-500 italic bg-slate-950/20 rounded-xl">
        Sin datos disponibles para graficar
      </div>
    );
  }

  return (
    <div
      ref={containerRef}
      className={`uplot-dark-container w-full h-full relative ${className}`}
      style={{ minHeight: height ? `${height}px` : '200px' }}
    />
  );
}

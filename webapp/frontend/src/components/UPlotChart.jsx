import React, { useEffect, useRef } from 'react';
import uPlot from 'uplot';
import 'uplot/dist/uPlot.min.css';

// Plugin de Tooltip Flotante de Alta Resolución para uPlot
function createTooltipPlugin() {
  let tooltipEl = null;

  return {
    hooks: {
      init(u) {
        tooltipEl = document.createElement('div');
        tooltipEl.className = 'uplot-floating-tooltip';
        Object.assign(tooltipEl.style, {
          display: 'none',
          position: 'absolute',
          background: 'rgba(15, 23, 42, 0.95)',
          border: '1px solid #334155',
          borderRadius: '8px',
          padding: '8px 12px',
          color: '#f8fafc',
          fontSize: '11px',
          fontFamily: 'Inter, system-ui, sans-serif',
          pointerEvents: 'none',
          zIndex: '40',
          boxShadow: '0 10px 25px -5px rgba(0, 0, 0, 0.6), 0 8px 10px -6px rgba(0, 0, 0, 0.6)',
          whiteSpace: 'nowrap',
          backdropFilter: 'blur(8px)',
          WebkitBackdropFilter: 'blur(8px)',
        });

        const over = u.root.querySelector('.u-over');
        if (over) {
          over.appendChild(tooltipEl);
        }
      },
      setCursor(u) {
        if (!tooltipEl) return;

        const { idx, left, top } = u.cursor;

        if (idx == null || idx === undefined || idx < 0 || left < 0 || top < 0) {
          tooltipEl.style.display = 'none';
          return;
        }

        const isTime = u.scales.x?.time !== false;
        const rawX = u.data[0][idx];

        if (rawX === undefined || rawX === null) {
          tooltipEl.style.display = 'none';
          return;
        }

        let xFormatted = '';
        if (isTime) {
          const date = new Date(rawX * 1000);
          const hh = String(date.getHours()).padStart(2, '0');
          const mm = String(date.getMinutes()).padStart(2, '0');
          const ss = String(date.getSeconds()).padStart(2, '0');
          const ms = String(date.getMilliseconds()).padStart(3, '0');
          xFormatted = `${hh}:${mm}:${ss}.${ms}`;
        } else {
          xFormatted = `${Number(rawX).toFixed(2)} Hz`;
        }

        let content = `<div style="font-weight: 600; color: #94a3b8; margin-bottom: 5px; border-bottom: 1px solid #1e293b; padding-bottom: 4px; display: flex; align-items: center; gap: 6px;">
          <span style="color: #38bdf8;">${isTime ? '⏱️' : '📊'}</span>
          <span>${isTime ? 'Tiempo: ' : 'Frecuencia: '} <strong style="color: #e2e8f0; font-family: monospace;">${xFormatted}</strong></span>
        </div>`;

        let hasData = false;
        for (let s = 1; s < u.series.length; s++) {
          const series = u.series[s];
          if (series.show === false) continue;

          const yVal = u.data[s] ? u.data[s][idx] : null;
          if (yVal !== undefined && yVal !== null) {
            hasData = true;
            const yFormatted = typeof yVal === 'number' ? yVal.toFixed(4) : yVal;
            const strokeColor = typeof series.stroke === 'function' ? series.stroke(u, s) : (series.stroke || '#38bdf8');
            const label = series.label || `Canal ${s}`;

            content += `<div style="display: flex; align-items: center; justify-content: space-between; gap: 14px; margin-top: 3px;">
              <span style="display: flex; align-items: center; gap: 6px; color: #cbd5e1;">
                <span style="display: inline-block; width: 8px; height: 8px; border-radius: 50%; background: ${strokeColor}; box-shadow: 0 0 6px ${strokeColor};"></span>
                <span>${label}:</span>
              </span>
              <span style="font-family: monospace; font-weight: 700; color: #f8fafc; text-align: right;">
                ${yFormatted}
              </span>
            </div>`;
          }
        }

        if (!hasData) {
          tooltipEl.style.display = 'none';
          return;
        }

        tooltipEl.innerHTML = content;
        tooltipEl.style.display = 'block';

        const chartWidth = u.bbox.width / (window.devicePixelRatio || 1);
        const chartHeight = u.bbox.height / (window.devicePixelRatio || 1);
        const tooltipWidth = tooltipEl.offsetWidth || 150;
        const tooltipHeight = tooltipEl.offsetHeight || 60;

        let xPos = left + 16;
        let yPos = top - 12;

        if (xPos + tooltipWidth > chartWidth) {
          xPos = left - tooltipWidth - 16;
        }
        if (yPos + tooltipHeight > chartHeight) {
          yPos = chartHeight - tooltipHeight - 8;
        }
        if (yPos < 0) {
          yPos = 8;
        }

        tooltipEl.style.left = `${Math.max(6, Math.floor(xPos))}px`;
        tooltipEl.style.top = `${Math.max(6, Math.floor(yPos))}px`;
      },
      destroy() {
        if (tooltipEl && tooltipEl.parentNode) {
          tooltipEl.parentNode.removeChild(tooltipEl);
          tooltipEl = null;
        }
      },
    },
  };
}

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

    const tooltip = createTooltipPlugin();

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
        live: true,
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
      plugins: [tooltip],
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
      plugins: [tooltip, ...(options?.plugins || [])],
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

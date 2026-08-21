import React from 'react';
import { Maximize2 } from 'lucide-react';
import { Line } from 'react-chartjs-2';

export default function ChartCard({ title, subtitle, data, options, onExpand, height = '260px' }) {
  return (
    <div className="bg-slate-900 border border-slate-800 rounded-2xl p-5 shadow-xl flex flex-col space-y-3 relative group">
      <div className="flex items-center justify-between border-b border-slate-800/80 pb-3">
        <div>
          <h3 className="text-xs font-bold uppercase tracking-wider text-slate-200">{title}</h3>
          {subtitle && <p className="text-[11px] text-slate-400 mt-0.5">{subtitle}</p>}
        </div>

        {onExpand && (
          <button
            onClick={onExpand}
            className="p-1.5 rounded-lg bg-slate-800 text-slate-400 hover:text-emerald-400 hover:bg-slate-700 transition-colors opacity-70 group-hover:opacity-100"
            title="Ver en pantalla completa"
          >
            <Maximize2 className="w-4 h-4" />
          </button>
        )}
      </div>

      <div style={{ height }} className="relative w-full">
        {data ? (
          <Line
            data={data}
            options={{
              ...options,
              responsive: true,
              maintainAspectRatio: false,
            }}
          />
        ) : (
          <div className="h-full flex items-center justify-center text-xs text-slate-600 italic">
            Sin datos para graficar
          </div>
        )}
      </div>
    </div>
  );
}

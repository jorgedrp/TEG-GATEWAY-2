import React from 'react';
import { X, Maximize2 } from 'lucide-react';
import { Line } from 'react-chartjs-2';

export default function FullscreenChartModal({ isOpen, onClose, title, chartData, chartOptions }) {
  if (!isOpen) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center p-4 sm:p-6 bg-black/85 backdrop-blur-md animate-in fade-in">
      <div className="bg-slate-900 border border-slate-700 rounded-2xl w-full h-[90vh] flex flex-col shadow-2xl overflow-hidden">
        {/* Header */}
        <div className="p-4 bg-slate-950 border-b border-slate-800 flex items-center justify-between">
          <div className="flex items-center space-x-2">
            <Maximize2 className="w-5 h-5 text-emerald-400" />
            <h3 className="text-base font-semibold text-white tracking-wide">{title} (Vista Detallada)</h3>
          </div>
          <button
            onClick={onClose}
            className="p-1.5 text-slate-400 hover:text-white rounded-lg hover:bg-slate-800 transition-colors"
          >
            <X className="w-6 h-6" />
          </button>
        </div>

        {/* Canvas container */}
        <div className="flex-1 p-6 relative bg-slate-950/40">
          {chartData && (
            <Line
              data={chartData}
              options={{
                ...chartOptions,
                responsive: true,
                maintainAspectRatio: false,
              }}
            />
          )}
        </div>
      </div>
    </div>
  );
}

import React from 'react';
import { Clock, X, Check, ShieldCheck } from 'lucide-react';

export default function SyncModal({ isOpen, onClose, onSync, targetDevice }) {
  if (!isOpen) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/70 backdrop-blur-sm animate-in fade-in">
      <div className="bg-slate-900 border border-slate-700 rounded-2xl max-w-md w-full p-6 shadow-2xl space-y-6">
        <div className="flex items-center justify-between border-b border-slate-800 pb-4">
          <div className="flex items-center space-x-3">
            <div className="p-2.5 bg-emerald-500/10 text-emerald-400 rounded-xl border border-emerald-500/30">
              <Clock className="w-6 h-6" />
            </div>
            <div>
              <h3 className="text-lg font-semibold text-white">Sincronización de Reloj</h3>
              <p className="text-xs text-slate-400">Sensor destino: {targetDevice === '255' ? 'Todos los sensores' : `Sensor ${targetDevice}`}</p>
            </div>
          </div>
          <button
            onClick={onClose}
            className="text-slate-400 hover:text-white p-1 rounded-lg hover:bg-slate-800 transition-colors"
          >
            <X className="w-5 h-5" />
          </button>
        </div>

        <p className="text-sm text-slate-300">
          Seleccione el algoritmo de sincronización temporal de precisión sub-milisegundo basado en timestamps por hardware:
        </p>

        <div className="grid grid-cols-1 gap-3">
          <button
            onClick={() => { onSync('simple'); onClose(); }}
            className="flex items-start space-x-3 p-4 rounded-xl border border-slate-700 bg-slate-800/60 hover:border-emerald-500/50 hover:bg-slate-800 transition-all text-left group"
          >
            <div className="p-2 rounded-lg bg-slate-700/50 group-hover:bg-emerald-500/20 group-hover:text-emerald-400 text-slate-300">
              <Check className="w-5 h-5" />
            </div>
            <div>
              <div className="font-medium text-white group-hover:text-emerald-400 transition-colors">Sincronización Simple</div>
              <div className="text-xs text-slate-400 mt-0.5">Compensación rápida de desfase (Offset) de 1 ciclo.</div>
            </div>
          </button>

          <button
            onClick={() => { onSync('completa'); onClose(); }}
            className="flex items-start space-x-3 p-4 rounded-xl border border-emerald-500/40 bg-emerald-950/20 hover:bg-emerald-950/40 hover:border-emerald-500 transition-all text-left group"
          >
            <div className="p-2 rounded-lg bg-emerald-500/20 text-emerald-400">
              <ShieldCheck className="w-5 h-5" />
            </div>
            <div>
              <div className="font-medium text-emerald-300 group-hover:text-emerald-200 transition-colors">Sincronización Completa</div>
              <div className="text-xs text-slate-400 mt-0.5">Estimación lineal de deriva (Skew) + Offset multietapa (Recomendado).</div>
            </div>
          </button>
        </div>

        <div className="flex justify-end pt-2">
          <button
            onClick={onClose}
            className="px-4 py-2 text-sm text-slate-400 hover:text-white rounded-lg hover:bg-slate-800 transition-colors"
          >
            Cancelar
          </button>
        </div>
      </div>
    </div>
  );
}

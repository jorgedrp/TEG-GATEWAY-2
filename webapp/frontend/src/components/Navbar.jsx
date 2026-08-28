import React from 'react';
import { Activity, Radio, Database, Sliders, List, LineChart, Square } from 'lucide-react';

export default function Navbar({ activeTab, setActiveTab, health, processStatus, onStopProcess }) {
  return (
    <header className="bg-slate-900/80 backdrop-blur border-b border-slate-800 sticky top-0 z-40">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="flex items-center justify-between h-16">
          {/* Brand */}
          <div className="flex items-center space-x-3">
            <div className="p-2 bg-emerald-500/10 border border-emerald-500/30 rounded-lg text-emerald-400">
              <Radio className="w-5 h-5 animate-pulse" />
            </div>
            <div>
              <div className="flex items-center space-x-2">
                <span className="font-bold text-lg tracking-tight text-white">TEG-GATEWAY</span>
                <span className="text-[10px] uppercase font-mono px-1.5 py-0.5 rounded bg-emerald-950 text-emerald-400 border border-emerald-800/50">
                  WSN-SHM v2.0
                </span>
              </div>
              <p className="text-xs text-slate-400">Gateway de monitoreo de salud estructural</p>
            </div>
          </div>

          {/* Navigation Tabs */}
          <nav className="flex items-center space-x-1 bg-slate-950/60 p-1 rounded-xl border border-slate-800">
            <button
              onClick={() => setActiveTab('config')}
              className={`flex items-center space-x-2 px-3.5 py-1.5 rounded-lg text-sm font-medium transition-all ${
                activeTab === 'config'
                  ? 'bg-emerald-500 text-slate-950 font-semibold shadow-lg shadow-emerald-500/20'
                  : 'text-slate-400 hover:text-white hover:bg-slate-800/50'
              }`}
            >
              <Sliders className="w-4 h-4" />
              <span>Configuración</span>
            </button>

            <button
              onClick={() => setActiveTab('registro')}
              className={`flex items-center space-x-2 px-3.5 py-1.5 rounded-lg text-sm font-medium transition-all ${
                activeTab === 'registro'
                  ? 'bg-emerald-500 text-slate-950 font-semibold shadow-lg shadow-emerald-500/20'
                  : 'text-slate-400 hover:text-white hover:bg-slate-800/50'
              }`}
            >
              <List className="w-4 h-4" />
              <span>Registro</span>
            </button>

            <button
              onClick={() => setActiveTab('explorar')}
              className={`flex items-center space-x-2 px-3.5 py-1.5 rounded-lg text-sm font-medium transition-all ${
                activeTab === 'explorar'
                  ? 'bg-emerald-500 text-slate-950 font-semibold shadow-lg shadow-emerald-500/20'
                  : 'text-slate-400 hover:text-white hover:bg-slate-800/50'
              }`}
            >
              <LineChart className="w-4 h-4" />
              <span>Explorar</span>
            </button>
          </nav>

          {/* System Status Indicators */}
          <div className="flex items-center space-x-3">
            {/* Process Status Badge */}
            {processStatus?.running ? (
              <div className="flex items-center space-x-2 bg-amber-500/10 border border-amber-500/30 px-2.5 py-1 rounded-lg">
                <span className="relative flex h-2 w-2">
                  <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-amber-400 opacity-75"></span>
                  <span className="relative inline-flex rounded-full h-2 w-2 bg-amber-500"></span>
                </span>
                <span className="text-xs font-mono text-amber-300 uppercase">
                  {processStatus.command} ({processStatus.uptime_seconds}s)
                </span>
                <button
                  onClick={onStopProcess}
                  title="Detener proceso actual"
                  className="ml-1 p-0.5 text-red-400 hover:text-red-300 hover:bg-red-950/50 rounded"
                >
                  <Square className="w-3.5 h-3.5 fill-current" />
                </button>
              </div>
            ) : (
              <div className="hidden sm:flex items-center space-x-1.5 px-2.5 py-1 rounded-lg bg-slate-800/50 border border-slate-700/50 text-slate-400 text-xs">
                <span className="h-1.5 w-1.5 rounded-full bg-slate-500"></span>
                <span>Inactivo</span>
              </div>
            )}

            {/* InfluxDB Status */}
            <div
              className={`flex items-center space-x-1.5 px-2.5 py-1 rounded-lg text-xs border ${
                health?.influxdb_connected
                  ? 'bg-emerald-950/40 text-emerald-400 border-emerald-800/60'
                  : 'bg-red-950/40 text-red-400 border-red-800/60'
              }`}
              title={health?.influxdb_connected ? `Conectado a InfluxDB (${health?.bucket})` : 'Sin conexión con InfluxDB'}
            >
              <Database className="w-3.5 h-3.5" />
              <span className="hidden md:inline">InfluxDB</span>
              <span className={`h-1.5 w-1.5 rounded-full ${health?.influxdb_connected ? 'bg-emerald-400 animate-pulse' : 'bg-red-500'}`} />
            </div>
          </div>
        </div>
      </div>
    </header>
  );
}

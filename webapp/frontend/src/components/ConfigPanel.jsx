import React, { useState, useEffect } from 'react';
import { Sliders, Radio, Activity, Clock, Cpu, RefreshCw, Send, CheckCircle2, AlertCircle } from 'lucide-react';
import { api } from '../services/api';

export default function ConfigPanel({ onOpenSync, onCommandSent }) {
  const [targetDevice, setTargetDevice] = useState('16');
  const [ventana, setVentana] = useState(35);
  const [umbral, setUmbral] = useState(0.20);
  const [frecuencia, setFrecuencia] = useState('5');
  const [lora, setLora] = useState('3');
  const [activeMode, setActiveMode] = useState('standby');
  const [sensorStates, setSensorStates] = useState({ '16': 'OFF', '32': 'OFF', '48': 'OFF', '64': 'OFF' });
  const [loading, setLoading] = useState(false);
  const [feedback, setFeedback] = useState(null);

  const SENSORS = [
    { id: '16', label: 'Sensor 1 (0x10)' },
    { id: '32', label: 'Sensor 2 (0x20)' },
    { id: '48', label: 'Sensor 3 (0x30)' },
    { id: '64', label: 'Sensor 4 (0x40)' },
  ];

  // Cargar configuración guardada al cambiar el sensor seleccionado
  useEffect(() => {
    if (targetDevice !== '255') {
      api.getNodes()
        .then((res) => {
          const found = res.nodes?.find((n) => n.node_id === targetDevice);
          if (found) {
            setFrecuencia(found.frequency || '5');
            setLora(found.lora_mode || '3');
            setVentana(parseInt(found.window_time) || 35);
            setUmbral(parseFloat(found.threshold) || 0.20);
          }
        })
        .catch(() => {});
    }
  }, [targetDevice]);

  // Actualizar estados periódicamente
  useEffect(() => {
    const fetchStates = async () => {
      try {
        const states = await api.getNodeStates();
        setSensorStates((prev) => ({ ...prev, ...states }));
      } catch (err) {
        // Silencioso en caso de desconexión momentánea
      }
    };

    fetchStates();
    const interval = setInterval(fetchStates, 2500);
    return () => clearInterval(interval);
  }, []);

  const handleSendCommand = async (cmd, payload = {}) => {
    setLoading(true);
    setFeedback(null);
    try {
      const res = await api.sendCommand(cmd, {
        device: targetDevice,
        ventana: String(ventana),
        umbral: String(umbral),
        frecuencia: String(frecuencia),
        lora: String(lora),
        ...payload
      });
      setFeedback({ type: 'success', message: res.message });
      if (onCommandSent) onCommandSent(cmd);
    } catch (err) {
      setFeedback({ type: 'error', message: err.message });
    } finally {
      setLoading(false);
    }
  };

  const handleConfigureOTA = () => {
    handleSendCommand('configurar');
  };

  const handleDetect = () => {
    handleSendCommand('detectar');
  };

  const getLedStyles = (mode) => {
    switch (mode) {
      case 'STANDBY':
        return { bg: 'bg-blue-500', glow: 'shadow-[0_0_12px_rgba(59,130,246,0.8)]', border: 'border-blue-400', text: 'text-blue-400' };
      case 'EVENTO':
        return { bg: 'bg-emerald-500', glow: 'shadow-[0_0_12px_rgba(34,197,94,0.8)]', border: 'border-emerald-400', text: 'text-emerald-400' };
      case 'TIEMPO':
        return { bg: 'bg-rose-500', glow: 'shadow-[0_0_12px_rgba(244,63,94,0.8)]', border: 'border-rose-400', text: 'text-rose-400' };
      case 'CLOCK':
        return { bg: 'bg-amber-400', glow: 'shadow-[0_0_12px_rgba(251,191,36,0.8)]', border: 'border-amber-300', text: 'text-amber-300' };
      default:
        return { bg: 'bg-slate-700', glow: '', border: 'border-slate-600', text: 'text-slate-500' };
    }
  };

  return (
    <div className="space-y-6">
      {/* Toast Feedback */}
      {feedback && (
        <div className={`p-4 rounded-xl border flex items-center space-x-3 text-sm animate-in fade-in ${
          feedback.type === 'success' ? 'bg-emerald-950/40 border-emerald-800 text-emerald-300' : 'bg-rose-950/40 border-rose-800 text-rose-300'
        }`}>
          {feedback.type === 'success' ? <CheckCircle2 className="w-5 h-5 flex-shrink-0" /> : <AlertCircle className="w-5 h-5 flex-shrink-0" />}
          <span>{feedback.message}</span>
        </div>
      )}

      {/* Grid de 2 Columnas: Parámetros OTA & Estado / Modos */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
        {/* Columna Izquierda: Parámetros de Medición y Radio LoRa (7 cols) */}
        <div className="lg:col-span-7 bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-xl space-y-6">
          <div className="flex items-center justify-between border-b border-slate-800 pb-4">
            <div className="flex items-center space-x-3">
              <div className="p-2 bg-emerald-500/10 text-emerald-400 rounded-xl border border-emerald-500/30">
                <Sliders className="w-5 h-5" />
              </div>
              <div>
                <h2 className="text-base font-semibold text-white">Parámetros de medición</h2>
                <p className="text-xs text-slate-400">Configuración remota dinámica sobre la red LoRa</p>
              </div>
            </div>

            {/* Selector de Sensor */}
            <div className="flex items-center space-x-2">
              <label className="text-xs text-slate-400 font-medium">Destino:</label>
              <select
                value={targetDevice}
                onChange={(e) => setTargetDevice(e.target.value)}
                className="bg-slate-800 text-white text-xs rounded-lg px-3 py-1.5 border border-slate-700 focus:outline-none focus:border-emerald-500"
              >
                <option value="16">Sensor 1</option>
                <option value="32">Sensor 2</option>
                <option value="48">Sensor 3</option>
                <option value="64">Sensor 4</option>
                <option value="255">Todos (Broadcast)</option>
              </select>
            </div>
          </div>

          <div className="space-y-5">
            {/* Slider: Tiempo de Medición */}
            <div>
              <div className="flex justify-between items-center mb-2">
                <label className="text-xs font-medium text-slate-300">Tiempo de medición (Ventana)</label>
                <span className="font-mono text-xs px-2 py-0.5 rounded bg-slate-800 text-emerald-400 font-bold">
                  {ventana} s
                </span>
              </div>
              <input
                type="range"
                min="2"
                max="254"
                step="2"
                value={ventana}
                onChange={(e) => setVentana(Number(e.target.value))}
                className="w-full h-2 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-emerald-500"
              />
              <div className="flex justify-between text-[10px] text-slate-500 font-mono mt-1">
                <span>2 s</span>
                <span>128 s</span>
                <span>254 s</span>
              </div>
            </div>

            {/* Slider: Umbral de Activación */}
            <div>
              <div className="flex justify-between items-center mb-2">
                <label className="text-xs font-medium text-slate-300">Umbral de activación por vibración</label>
                <span className="font-mono text-xs px-2 py-0.5 rounded bg-slate-800 text-emerald-400 font-bold">
                  {Number(umbral).toFixed(2)} g
                </span>
              </div>
              <input
                type="range"
                min="0.10"
                max="1.00"
                step="0.01"
                value={umbral}
                onChange={(e) => setUmbral(Number(e.target.value))}
                className="w-full h-2 bg-slate-800 rounded-lg appearance-none cursor-pointer accent-emerald-500"
              />
              <div className="flex justify-between text-[10px] text-slate-500 font-mono mt-1">
                <span>0.10 g</span>
                <span>0.50 g</span>
                <span>1.00 g</span>
              </div>
            </div>

            {/* Frecuencia y LoRa Dropdowns */}
            <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
              <div>
                <label className="block text-xs font-medium text-slate-300 mb-1.5">Frecuencia de muestreo</label>
                <select
                  value={frecuencia}
                  onChange={(e) => setFrecuencia(e.target.value)}
                  className="w-full bg-slate-800 text-white text-xs rounded-xl p-3 border border-slate-700 focus:outline-none focus:border-emerald-500"
                >
                  <option value="10">100 Hz</option>
                  <option value="5">200 Hz</option>
                  <option value="4">250 Hz</option>
                  <option value="2">500 Hz</option>
                  <option value="1">1000 Hz</option>
                </select>
              </div>

              <div>
                <label className="block text-xs font-medium text-slate-300 mb-1.5">Modulación LoRa (SF / BW)</label>
                <select
                  value={lora}
                  onChange={(e) => setLora(e.target.value)}
                  className="w-full bg-slate-800 text-white text-xs rounded-xl p-3 border border-slate-700 focus:outline-none focus:border-emerald-500"
                >
                  <option value="1">SF7 - BW125 kHz</option>
                  <option value="2">SF7 - BW250 kHz</option>
                  <option value="3">SF7 - BW500 kHz</option>
                  <option value="4">SF8 - BW125 kHz</option>
                  <option value="5">SF9 - BW125 kHz</option>
                  <option value="6">SF10 - BW125 kHz</option>
                </select>
              </div>
            </div>

            {/* Acciones OTA */}
            <div className="pt-2 flex flex-wrap gap-3">
              <button
                onClick={handleConfigureOTA}
                disabled={loading}
                className="flex-1 flex items-center justify-center space-x-2 bg-emerald-500 hover:bg-emerald-600 disabled:opacity-50 text-slate-950 font-semibold text-xs py-3 px-4 rounded-xl shadow-lg shadow-emerald-500/20 transition-all"
              >
                <Send className="w-4 h-4" />
                <span>Aplicar Configuración</span>
              </button>

              <button
                onClick={() => onOpenSync(targetDevice)}
                disabled={loading}
                className="flex items-center justify-center space-x-2 bg-slate-800 hover:bg-slate-700 text-white text-xs py-3 px-4 rounded-xl border border-slate-700 transition-colors"
              >
                <Clock className="w-4 h-4 text-emerald-400" />
                <span>Sincronizar Reloj</span>
              </button>
            </div>
          </div>
        </div>

        {/* Columna Derecha: Estado de Nodos y Lanzador de Modos (5 cols) */}
        <div className="lg:col-span-5 space-y-6">
          {/* Card de Estado de Sensores */}
          <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-xl space-y-4">
            <div className="flex items-center justify-between border-b border-slate-800 pb-3">
              <div className="flex items-center space-x-2">
                <Activity className="w-4 h-4 text-emerald-400" />
                <h3 className="text-sm font-semibold text-white">Estado de los sensores</h3>
              </div>
              <button
                onClick={handleDetect}
                disabled={loading}
                className="flex items-center space-x-1 text-xs px-2.5 py-1 rounded-lg bg-slate-800 hover:bg-slate-700 text-slate-300 transition-colors"
              >
                <RefreshCw className={`w-3.5 h-3.5 ${loading ? 'animate-spin' : ''}`} />
                <span>Detectar</span>
              </button>
            </div>

            {/* Grid de LEDs de Sensores */}
            <div className="grid grid-cols-2 sm:grid-cols-4 gap-3">
              {SENSORS.map((s) => {
                const mode = sensorStates[s.id] || 'OFF';
                const style = getLedStyles(mode);
                return (
                  <div key={s.id} className="bg-slate-950/60 border border-slate-800/80 rounded-xl p-3 flex flex-col items-center justify-center space-y-2">
                    <div className={`w-4 h-4 rounded-full ${style.bg} ${style.glow} border ${style.border} transition-all duration-500`} />
                    <span className="text-xs font-medium text-white">Nodo {s.id}</span>
                    <span className={`text-[10px] font-mono uppercase font-bold ${style.text}`}>
                      {mode}
                    </span>
                  </div>
                );
              })}
            </div>

            <div className="flex items-center justify-between text-[10px] text-slate-500 font-mono pt-1">
              <span className="flex items-center space-x-1"><span className="w-2 h-2 rounded-full bg-blue-500 inline-block"></span> <span>Standby</span></span>
              <span className="flex items-center space-x-1"><span className="w-2 h-2 rounded-full bg-emerald-500 inline-block"></span> <span>Evento</span></span>
              <span className="flex items-center space-x-1"><span className="w-2 h-2 rounded-full bg-rose-500 inline-block"></span> <span>Tiempo</span></span>
              <span className="flex items-center space-x-1"><span className="w-2 h-2 rounded-full bg-amber-400 inline-block"></span> <span>Clock</span></span>
            </div>
          </div>

          {/* Card de Lanzamiento de Modos Operativos */}
          <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-xl space-y-4">
            <div className="flex items-center space-x-2 border-b border-slate-800 pb-3">
              <Radio className="w-4 h-4 text-emerald-400" />
              <h3 className="text-sm font-semibold text-white">Modos de operación</h3>
            </div>

            <div className="grid grid-cols-2 gap-2.5">
              <button
                onClick={() => { setActiveMode('standby'); handleSendCommand('standby'); }}
                disabled={loading}
                className={`py-3 px-3 rounded-xl font-medium text-xs border transition-all flex flex-col items-center justify-center space-y-1 ${
                  activeMode === 'standby'
                    ? 'bg-blue-950/40 border-blue-500 text-blue-300 shadow-lg shadow-blue-500/10'
                    : 'bg-slate-800/40 border-slate-800 text-slate-400 hover:text-white hover:bg-slate-800'
                }`}
              >
                <span className="font-semibold text-sm">Standby</span>
                <span className="text-[10px] text-slate-500">En reposo</span>
              </button>

              <button
                onClick={() => { setActiveMode('eventos'); handleSendCommand('eventos'); }}
                disabled={loading}
                className={`py-3 px-3 rounded-xl font-medium text-xs border transition-all flex flex-col items-center justify-center space-y-1 ${
                  activeMode === 'eventos'
                    ? 'bg-emerald-950/40 border-emerald-500 text-emerald-300 shadow-lg shadow-emerald-500/10'
                    : 'bg-slate-800/40 border-slate-800 text-slate-400 hover:text-white hover:bg-slate-800'
                }`}
              >
                <span className="font-semibold text-sm">Eventos</span>
                <span className="text-[10px] text-slate-500">Disparo por umbral</span>
              </button>

              <button
                onClick={() => { setActiveMode('tiempo'); handleSendCommand('tiempo'); }}
                disabled={loading}
                className={`py-3 px-3 rounded-xl font-medium text-xs border transition-all flex flex-col items-center justify-center space-y-1 ${
                  activeMode === 'tiempo'
                    ? 'bg-rose-950/40 border-rose-500 text-rose-300 shadow-lg shadow-rose-500/10'
                    : 'bg-slate-800/40 border-slate-800 text-slate-400 hover:text-white hover:bg-slate-800'
                }`}
              >
                <span className="font-semibold text-sm">Tiempo</span>
                <span className="text-[10px] text-slate-500">Disparo manual</span>
              </button>

              <button
                onClick={() => { setActiveMode('programado'); handleSendCommand('programado'); }}
                disabled={loading}
                className={`py-3 px-3 rounded-xl font-medium text-xs border transition-all flex flex-col items-center justify-center space-y-1 ${
                  activeMode === 'programado'
                    ? 'bg-amber-950/40 border-amber-500 text-amber-300 shadow-lg shadow-amber-500/10'
                    : 'bg-slate-800/40 border-slate-800 text-slate-400 hover:text-white hover:bg-slate-800'
                }`}
              >
                <span className="font-semibold text-sm">Programado</span>
                <span className="text-[10px] text-slate-500">Disparo recurrente</span>
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

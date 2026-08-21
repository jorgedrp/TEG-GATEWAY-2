import React, { useState, useEffect } from 'react';
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
  Filler,
} from 'chart.js';
import { Play, RotateCcw, Filter, Activity, Thermometer, Droplets, Gauge, AlertCircle, Zap } from 'lucide-react';
import { api } from '../services/api';
import ChartCard from './ChartCard';
import FullscreenChartModal from './FullscreenChartModal';

// Registrar componentes de Chart.js
ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
  Filler
);

export default function AnalyticsExplorer({ initialParams }) {
  const [sensor, setSensor] = useState('16');
  const [channel, setChannel] = useState('ax');
  const [startTime, setStartTime] = useState('');
  const [stopTime, setStopTime] = useState('');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);

  // Datos recibidos
  const [telemetryData, setTelemetryData] = useState(null);
  const [inclinacionData, setInclinacionData] = useState(null);

  // Control de filtro de frecuencias para FFT
  const [minFreq, setMinFreq] = useState(0);
  const [maxFreq, setMaxFreq] = useState(50);
  const [appliedFreqRange, setAppliedFreqRange] = useState({ min: 0, max: 50 });

  // Modal Fullscreen
  const [modalConfig, setModalConfig] = useState({ isOpen: false, title: '', data: null, options: null });

  // Inicializar con fecha actual o parámetros pasados desde pestaña de Registro
  useEffect(() => {
    if (initialParams?.sensor) {
      setSensor(initialParams.sensor);
    }

    if (initialParams?.inicio && initialParams?.fin) {
      // Convertir ms a formato datetime-local
      const toLocalISO = (ms) => {
        const d = new Date(parseInt(ms));
        const offset = d.getTimezoneOffset() * 60000;
        return new Date(d.getTime() - offset).toISOString().slice(0, 19);
      };
      setStartTime(toLocalISO(initialParams.inicio));
      setStopTime(toLocalISO(initialParams.fin));
    } else {
      // Default: Últimos 5 minutos
      const now = new Date();
      const fiveMinAgo = new Date(now.getTime() - 5 * 60 * 1000);
      const offset = now.getTimezoneOffset() * 60000;
      setStopTime(new Date(now.getTime() - offset).toISOString().slice(0, 19));
      setStartTime(new Date(fiveMinAgo.getTime() - offset).toISOString().slice(0, 19));
    }
  }, [initialParams]);

  const handleApplyPreset = (minutes) => {
    const now = new Date();
    const past = new Date(now.getTime() - minutes * 60 * 1000);
    const offset = now.getTimezoneOffset() * 60000;
    setStopTime(new Date(now.getTime() - offset).toISOString().slice(0, 19));
    setStartTime(new Date(past.getTime() - offset).toISOString().slice(0, 19));
  };

  const handleFetchData = async (e) => {
    if (e) e.preventDefault();
    setLoading(true);
    setError(null);

    try {
      const startISO = startTime ? new Date(startTime).toISOString() : '-5m';
      const stopISO = stopTime ? new Date(stopTime).toISOString() : 'now()';

      // 1. Telemetría Aceleración / Ambiente / FFT
      const telPromise = api.getTelemetryData(startISO, stopISO, channel, sensor);
      // 2. Inclinación Kalman 6-DoF
      const incPromise = api.getInclinacionData(startISO, stopISO, sensor).catch(() => null);

      const [telRes, incRes] = await Promise.all([telPromise, incPromise]);
      setTelemetryData(telRes);
      setInclinacionData(incRes);
    } catch (err) {
      setError(err.message || 'Error al consultar datos de telemetría.');
      setTelemetryData(null);
      setInclinacionData(null);
    } finally {
      setLoading(false);
    }
  };

  // 1. Time Chart Data
  const timeChartData = telemetryData ? {
    labels: telemetryData.timeSeries.time,
    datasets: [{
      label: `Registro ${channel.toUpperCase()}`,
      data: telemetryData.timeSeries.data,
      borderColor: 'rgb(56, 189, 248)',
      backgroundColor: 'rgba(56, 189, 248, 0.1)',
      borderWidth: 1.5,
      pointRadius: 0.8,
      fill: true,
    }]
  } : null;

  const timeChartOptions = {
    scales: {
      x: { title: { display: true, text: 'Tiempo (Local)', color: '#94a3b8' }, ticks: { color: '#64748b', maxTicksLimit: 10 }, grid: { color: '#1e293b' } },
      y: { title: { display: true, text: channel.startsWith('g') ? 'Velocidad Angular (°/s)' : 'Aceleración (g)', color: '#94a3b8' }, ticks: { color: '#64748b' }, grid: { color: '#1e293b' } }
    },
    plugins: { legend: { display: false } }
  };

  // 2. FFT Chart Data (Filtrado por rango de frecuencia)
  const filteredFreqIndices = telemetryData?.frequencySeries?.frequencies
    ?.map((f, i) => (f >= appliedFreqRange.min && f <= appliedFreqRange.max ? i : -1))
    ?.filter((i) => i !== -1) || [];

  const freqChartData = telemetryData ? {
    labels: filteredFreqIndices.map((i) => telemetryData.frequencySeries.frequencies[i]),
    datasets: [{
      label: 'Magnitud Espectral (ACF Scaled)',
      data: filteredFreqIndices.map((i) => telemetryData.frequencySeries.magnitude[i]),
      borderColor: 'rgb(244, 63, 94)',
      backgroundColor: 'rgba(244, 63, 94, 0.15)',
      borderWidth: 1.5,
      pointRadius: 1,
      fill: true,
    }]
  } : null;

  const freqChartOptions = {
    scales: {
      x: { title: { display: true, text: 'Frecuencia (Hz)', color: '#94a3b8' }, ticks: { color: '#64748b', maxTicksLimit: 12 }, grid: { color: '#1e293b' } },
      y: { title: { display: true, text: 'Magnitud Espectral', color: '#94a3b8' }, beginAtZero: true, ticks: { color: '#64748b' }, grid: { color: '#1e293b' } }
    },
    plugins: { legend: { display: false } }
  };

  // 3. Pitch Chart
  const pitchChartData = inclinacionData ? {
    labels: inclinacionData.labels,
    datasets: [{
      label: 'Inclinación Pitch (°)',
      data: inclinacionData.pitch,
      borderColor: 'rgb(251, 146, 60)',
      backgroundColor: 'rgba(251, 146, 60, 0.1)',
      borderWidth: 1.5,
      pointRadius: 1,
    }]
  } : null;

  // 4. Roll Chart
  const rollChartData = inclinacionData ? {
    labels: inclinacionData.labels,
    datasets: [{
      label: 'Inclinación Roll (°)',
      data: inclinacionData.roll,
      borderColor: 'rgb(168, 85, 247)',
      backgroundColor: 'rgba(168, 85, 247, 0.1)',
      borderWidth: 1.5,
      pointRadius: 1,
    }]
  } : null;

  const angleOptions = {
    scales: {
      x: { ticks: { color: '#64748b', maxTicksLimit: 8 }, grid: { color: '#1e293b' } },
      y: { title: { display: true, text: 'Ángulo (°)', color: '#94a3b8' }, ticks: { color: '#64748b' }, grid: { color: '#1e293b' } }
    },
    plugins: { legend: { display: false } }
  };

  // 5. Temperature Chart
  const tempChartData = telemetryData?.ambSeries ? {
    labels: telemetryData.ambSeries.time,
    datasets: [{
      label: 'Temperatura (°C)',
      data: telemetryData.ambSeries.temp,
      borderColor: 'rgb(239, 68, 68)',
      backgroundColor: 'rgba(239, 68, 68, 0.1)',
      borderWidth: 1.5,
      pointRadius: 1,
    }]
  } : null;

  // 6. Humidity Chart
  const humeChartData = telemetryData?.ambSeries ? {
    labels: telemetryData.ambSeries.time,
    datasets: [{
      label: 'Humedad (%)',
      data: telemetryData.ambSeries.hum,
      borderColor: 'rgb(59, 130, 246)',
      backgroundColor: 'rgba(59, 130, 246, 0.1)',
      borderWidth: 1.5,
      pointRadius: 1,
    }]
  } : null;

  // 7. Pressure Chart
  const presChartData = telemetryData?.ambSeries ? {
    labels: telemetryData.ambSeries.time,
    datasets: [{
      label: 'Presión (hPa)',
      data: telemetryData.ambSeries.pres,
      borderColor: 'rgb(34, 197, 94)',
      backgroundColor: 'rgba(34, 197, 94, 0.1)',
      borderWidth: 1.5,
      pointRadius: 1,
    }]
  } : null;

  const envOptions = (unit) => ({
    scales: {
      x: { ticks: { color: '#64748b', maxTicksLimit: 8 }, grid: { color: '#1e293b' } },
      y: { title: { display: true, text: unit, color: '#94a3b8' }, ticks: { color: '#64748b' }, grid: { color: '#1e293b' } }
    },
    plugins: { legend: { display: false } }
  });

  const openFullscreen = (title, data, options) => {
    setModalConfig({ isOpen: true, title, data, options });
  };

  return (
    <div className="space-y-6">
      {/* Controles de Consulta y Filtros */}
      <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-xl space-y-4">
        <form onSubmit={handleFetchData} className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-5 gap-4 items-end">
          {/* Sensor */}
          <div>
            <label className="block text-xs font-medium text-slate-300 mb-1.5">Sensor</label>
            <select
              value={sensor}
              onChange={(e) => setSensor(e.target.value)}
              className="w-full bg-slate-800 text-white text-xs rounded-xl p-2.5 border border-slate-700 focus:outline-none focus:border-emerald-500"
            >
              <option value="16">Sensor 1 (0x10)</option>
              <option value="32">Sensor 2 (0x20)</option>
              <option value="48">Sensor 3 (0x30)</option>
              <option value="64">Sensor 4 (0x40)</option>
            </select>
          </div>

          {/* Canal / Eje */}
          <div>
            <label className="block text-xs font-medium text-slate-300 mb-1.5">Canal / Variable</label>
            <select
              value={channel}
              onChange={(e) => setChannel(e.target.value)}
              className="w-full bg-slate-800 text-white text-xs rounded-xl p-2.5 border border-slate-700 focus:outline-none focus:border-emerald-500"
            >
              <option value="ax">Aceleración Ax (g)</option>
              <option value="ay">Aceleración Ay (g)</option>
              <option value="az">Aceleración Az (g)</option>
              <option value="gx">Giroscopio Gx (°/s)</option>
              <option value="gy">Giroscopio Gy (°/s)</option>
              <option value="gz">Giroscopio Gz (°/s)</option>
            </select>
          </div>

          {/* Fecha Inicio */}
          <div>
            <label className="block text-xs font-medium text-slate-300 mb-1.5">Inicio (Local)</label>
            <input
              type="datetime-local"
              step="1"
              value={startTime}
              onChange={(e) => setStartTime(e.target.value)}
              className="w-full bg-slate-800 text-white text-xs rounded-xl p-2.5 border border-slate-700 focus:outline-none focus:border-emerald-500 font-mono"
            />
          </div>

          {/* Fecha Fin */}
          <div>
            <label className="block text-xs font-medium text-slate-300 mb-1.5">Fin (Local)</label>
            <input
              type="datetime-local"
              step="1"
              value={stopTime}
              onChange={(e) => setStopTime(e.target.value)}
              className="w-full bg-slate-800 text-white text-xs rounded-xl p-2.5 border border-slate-700 focus:outline-none focus:border-emerald-500 font-mono"
            />
          </div>

          {/* Botón Ejecutar */}
          <div>
            <button
              type="submit"
              disabled={loading}
              className="w-full flex items-center justify-center space-x-2 bg-emerald-500 hover:bg-emerald-600 disabled:opacity-50 text-slate-950 font-semibold text-xs py-2.5 px-4 rounded-xl shadow-lg shadow-emerald-500/20 transition-all"
            >
              <Play className={`w-4 h-4 fill-current ${loading ? 'animate-spin' : ''}`} />
              <span>{loading ? 'Consultando...' : 'Graficar Telemetría'}</span>
            </button>
          </div>
        </form>

        {/* Presets Rápidos y Filtro Espectral */}
        <div className="pt-3 border-t border-slate-800/80 flex flex-wrap items-center justify-between gap-4 text-xs">
          {/* Presets */}
          <div className="flex items-center space-x-2">
            <span className="text-slate-400 font-medium">Rango rápido:</span>
            <button onClick={() => handleApplyPreset(5)} className="px-2.5 py-1 rounded-lg bg-slate-800 hover:bg-slate-700 text-slate-300 transition-colors">
              5 min
            </button>
            <button onClick={() => handleApplyPreset(60)} className="px-2.5 py-1 rounded-lg bg-slate-800 hover:bg-slate-700 text-slate-300 transition-colors">
              1 hora
            </button>
            <button onClick={() => handleApplyPreset(1440)} className="px-2.5 py-1 rounded-lg bg-slate-800 hover:bg-slate-700 text-slate-300 transition-colors">
              24 horas
            </button>
          </div>

          {/* Filtro de Rango FFT */}
          <div className="flex items-center space-x-2">
            <span className="text-slate-400 font-medium">Filtro FFT (Hz):</span>
            <input
              type="number"
              min="0"
              max="500"
              value={minFreq}
              onChange={(e) => setMinFreq(Number(e.target.value))}
              className="w-16 bg-slate-800 text-white px-2 py-1 rounded-lg border border-slate-700 text-xs font-mono"
              placeholder="Min"
            />
            <span className="text-slate-500">-</span>
            <input
              type="number"
              min="1"
              max="500"
              value={maxFreq}
              onChange={(e) => setMaxFreq(Number(e.target.value))}
              className="w-16 bg-slate-800 text-white px-2 py-1 rounded-lg border border-slate-700 text-xs font-mono"
              placeholder="Max"
            />
            <button
              onClick={() => setAppliedFreqRange({ min: minFreq, max: maxFreq })}
              className="px-2.5 py-1 rounded-lg bg-emerald-950 text-emerald-400 border border-emerald-800/60 font-medium hover:bg-emerald-900 transition-colors"
            >
              Aplicar
            </button>
            <button
              onClick={() => { setMinFreq(0); setMaxFreq(50); setAppliedFreqRange({ min: 0, max: 50 }); }}
              className="p-1 text-slate-400 hover:text-white rounded-lg hover:bg-slate-800"
              title="Restablecer a 0-50 Hz"
            >
              <RotateCcw className="w-3.5 h-3.5" />
            </button>
          </div>
        </div>
      </div>

      {/* Alerta de Error */}
      {error && (
        <div className="p-4 rounded-xl border border-rose-800 bg-rose-950/40 text-rose-300 text-sm flex items-center space-x-3">
          <AlertCircle className="w-5 h-5 flex-shrink-0" />
          <span>{error}</span>
        </div>
      )}

      {/* Resonant Frequency Highlight Banner */}
      {telemetryData?.frequencySeries?.peak_frequency && (
        <div className="p-4 bg-emerald-950/20 border border-emerald-500/30 rounded-2xl flex items-center justify-between text-xs">
          <div className="flex items-center space-x-2.5 text-emerald-400">
            <Zap className="w-4 h-4 text-emerald-400 animate-pulse" />
            <span className="font-semibold">Frecuencia Resonante Modal Principal Detectada:</span>
            <span className="font-mono font-bold text-sm bg-emerald-900/60 px-2 py-0.5 rounded text-white border border-emerald-700/50">
              {telemetryData.frequencySeries.peak_frequency} Hz
            </span>
          </div>
          <span className="text-slate-400 hidden sm:inline">Espectro FFT con ventaneo Hann y corrección ACF</span>
        </div>
      )}

      {/* Grid Principal de Gráficos (Vibración y FFT) */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <ChartCard
          title={`Registro en el Tiempo (${channel.toUpperCase()})`}
          subtitle="Señal de aceleración / giroscopio cruda de alta frecuencia"
          data={timeChartData}
          options={timeChartOptions}
          height="280px"
          onExpand={() => openFullscreen(`Registro en el Tiempo (${channel.toUpperCase()})`, timeChartData, timeChartOptions)}
        />

        <ChartCard
          title="Espectro de Frecuencia (FFT)"
          subtitle={`Densidad espectral con ventana Hann (Rango: ${appliedFreqRange.min} - ${appliedFreqRange.max} Hz)`}
          data={freqChartData}
          options={freqChartOptions}
          height="280px"
          onExpand={() => openFullscreen('Espectro de Frecuencia (FFT)', freqChartData, freqChartOptions)}
        />
      </div>

      {/* Grid Secundario: Inclinometría (Filtro de Kalman 6-DoF) */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <ChartCard
          title="Inclinación Pitch (Filtro de Kalman)"
          subtitle="Estimación angular sobre el eje transversal Y sin deriva de giroscopio"
          data={pitchChartData}
          options={angleOptions}
          height="220px"
          onExpand={() => openFullscreen('Inclinación Pitch (Filtro de Kalman)', pitchChartData, angleOptions)}
        />

        <ChartCard
          title="Inclinación Roll (Filtro de Kalman)"
          subtitle="Estimación angular sobre el eje longitudinal X sin deriva de giroscopio"
          data={rollChartData}
          options={angleOptions}
          height="220px"
          onExpand={() => openFullscreen('Inclinación Roll (Filtro de Kalman)', rollChartData, angleOptions)}
        />
      </div>

      {/* Grid Terciario: Variables Ambientales BME280 */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
        <ChartCard
          title="Temperatura BME280"
          subtitle="Sensor ambiental digital (°C)"
          data={tempChartData}
          options={envOptions('Temperatura (°C)')}
          height="180px"
          onExpand={() => openFullscreen('Temperatura Ambiental (°C)', tempChartData, envOptions('Temperatura (°C)'))}
        />

        <ChartCard
          title="Humedad Relativa"
          subtitle="Humedad porcentual (%)"
          data={humeChartData}
          options={envOptions('Humedad (%)')}
          height="180px"
          onExpand={() => openFullscreen('Humedad Relativa (%)', humeChartData, envOptions('Humedad (%)'))}
        />

        <ChartCard
          title="Presión Barométrica"
          subtitle="Presión atmosférica (hPa)"
          data={presChartData}
          options={envOptions('Presión (hPa)')}
          height="180px"
          onExpand={() => openFullscreen('Presión Barométrica (hPa)', presChartData, envOptions('Presión (hPa)'))}
        />
      </div>

      {/* Modal de Pantalla Completa */}
      <FullscreenChartModal
        isOpen={modalConfig.isOpen}
        onClose={() => setModalConfig({ ...modalConfig, isOpen: false })}
        title={modalConfig.title}
        chartData={modalConfig.data}
        chartOptions={modalConfig.options}
      />
    </div>
  );
}

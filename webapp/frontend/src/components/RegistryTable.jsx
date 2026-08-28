import React, { useState, useEffect } from 'react';
import { Download, Search, Filter, LineChart, Trash2, Calendar, Clock, AlertCircle } from 'lucide-react';
import { api } from '../services/api';

export default function RegistryTable({ onExploreRecord }) {
  const [records, setRecords] = useState([]);
  const [loading, setLoading] = useState(true);
  const [page, setPage] = useState(1);
  const [totalPages, setTotalPages] = useState(1);
  const [totalRecords, setTotalRecords] = useState(0);
  const [selectedSensor, setSelectedSensor] = useState('255');
  const [searchFilter, setSearchFilter] = useState('');

  const fetchRecords = async () => {
    setLoading(true);
    try {
      const res = await api.getRegistry(page, 25, selectedSensor);
      setRecords(res.records || []);
      setTotalPages(res.total_pages || 1);
      setTotalRecords(res.total || 0);
    } catch (err) {
      console.error('Error al cargar registros:', err);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchRecords();
  }, [page, selectedSensor]);

  const handleDelete = async (id) => {
    if (window.confirm(`¿Estás seguro de eliminar el registro #${id}?`)) {
      try {
        await api.deleteRegistry(id);
        fetchRecords();
      } catch (err) {
        alert('Error al eliminar registro');
      }
    }
  };

  const filteredRecords = records.filter((r) => {
    if (!searchFilter) return true;
    const term = searchFilter.toLowerCase();
    return (
      String(r.id).includes(term) ||
      String(r.node_id).includes(term) ||
      (r.inicio_legible && r.inicio_legible.toLowerCase().includes(term)) ||
      (r.fin_legible && r.fin_legible.toLowerCase().includes(term)) ||
      (r.record_type && r.record_type.toLowerCase().includes(term))
    );
  });

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-2xl shadow-xl overflow-hidden space-y-4">
      {/* Header & Controls */}
      <div className="p-6 border-b border-slate-800 flex flex-col md:flex-row md:items-center md:justify-between gap-4">
        <div>
          <h2 className="text-lg font-semibold text-white">Registro de mediciones y eventos</h2>
          <p className="text-xs text-slate-400">Historial de capturas registradas por los sensores</p>
        </div>

        <div className="flex flex-wrap items-center gap-3">
          {/* Sensor Filter */}
          <div className="flex items-center space-x-2">
            <Filter className="w-4 h-4 text-slate-400" />
            <select
              value={selectedSensor}
              onChange={(e) => { setSelectedSensor(e.target.value); setPage(1); }}
              className="bg-slate-800 text-white text-xs rounded-xl px-3 py-2 border border-slate-700 focus:outline-none focus:border-emerald-500"
            >
              <option value="255">Todos los Sensores</option>
              <option value="16">Sensor 1 (16)</option>
              <option value="32">Sensor 2 (32)</option>
              <option value="48">Sensor 3 (48)</option>
              <option value="64">Sensor 4 (64)</option>
            </select>
          </div>

          {/* Search Box */}
          <div className="relative">
            <Search className="w-3.5 h-3.5 text-slate-400 absolute left-3 top-1/2 -translate-y-1/2" />
            <input
              type="text"
              placeholder="Buscar por fecha, ID..."
              value={searchFilter}
              onChange={(e) => setSearchFilter(e.target.value)}
              className="bg-slate-800 text-white text-xs rounded-xl pl-9 pr-3 py-2 border border-slate-700 focus:outline-none focus:border-emerald-500 w-48"
            />
          </div>

          {/* Export CSV Button */}
          <a
            href={`/api/registry/export${selectedSensor !== '255' ? `?node=${selectedSensor}` : ''}`}
            download
            className="flex items-center space-x-1.5 px-3.5 py-2 rounded-xl bg-slate-800 hover:bg-slate-700 text-emerald-400 border border-slate-700 text-xs font-medium transition-colors"
          >
            <Download className="w-4 h-4" />
            <span>Exportar CSV</span>
          </a>
        </div>
      </div>

      {/* Tabla de Registros */}
      <div className="overflow-x-auto">
        <table className="w-full text-left text-xs">
          <thead className="bg-slate-950/80 text-slate-400 uppercase tracking-wider font-mono border-b border-slate-800">
            <tr>
              <th className="py-3.5 px-4 font-semibold"># ID</th>
              <th className="py-3.5 px-4 font-semibold">Sensor</th>
              <th className="py-3.5 px-4 font-semibold">Inicio (Local)</th>
              <th className="py-3.5 px-4 font-semibold">Fin (Local)</th>
              <th className="py-3.5 px-4 font-semibold">Duración</th>
              <th className="py-3.5 px-4 font-semibold">Tipo</th>
              <th className="py-3.5 px-4 font-semibold text-right">Acciones</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-800/60 font-sans">
            {loading ? (
              <tr>
                <td colSpan="7" className="py-12 text-center text-slate-500 italic">
                  Cargando catálogo de mediciones...
                </td>
              </tr>
            ) : filteredRecords.length === 0 ? (
              <tr>
                <td colSpan="7" className="py-12 text-center text-slate-500">
                  <div className="flex flex-col items-center justify-center space-y-2">
                    <AlertCircle className="w-8 h-8 text-slate-600" />
                    <span>No hay registros disponibles para el criterio seleccionado.</span>
                  </div>
                </td>
              </tr>
            ) : (
              filteredRecords.map((r) => (
                <tr key={r.id} className="hover:bg-slate-800/40 transition-colors group">
                  <td className="py-3 px-4 font-mono text-slate-400 font-medium">#{r.id}</td>
                  <td className="py-3 px-4">
                    <span className="inline-flex items-center px-2 py-0.5 rounded-full text-xs font-medium bg-emerald-950 text-emerald-400 border border-emerald-800/60 font-mono">
                      Nodo {r.node_id}
                    </span>
                  </td>
                  <td className="py-3 px-4 text-slate-200">{r.inicio_legible}</td>
                  <td className="py-3 px-4 text-slate-200">{r.fin_legible}</td>
                  <td className="py-3 px-4 font-mono text-emerald-400">{r.duration_seconds} s</td>
                  <td className="py-3 px-4">
                    <span className="text-[10px] font-mono uppercase px-1.5 py-0.5 rounded bg-slate-800 text-slate-300">
                      {r.record_type}
                    </span>
                  </td>
                  <td className="py-3 px-4 text-right space-x-2">
                    <button
                      onClick={() => onExploreRecord(r)}
                      className="inline-flex items-center space-x-1 px-2.5 py-1.5 rounded-lg bg-emerald-500/10 hover:bg-emerald-500 text-emerald-400 hover:text-slate-950 font-semibold border border-emerald-500/30 transition-all text-xs"
                      title="Analizar serie temporal y FFT en Explorador"
                    >
                      <LineChart className="w-3.5 h-3.5" />
                      <span>Explorar</span>
                    </button>

                    <button
                      onClick={() => handleDelete(r.id)}
                      className="p-1.5 text-slate-500 hover:text-rose-400 hover:bg-rose-950/40 rounded-lg transition-colors"
                      title="Eliminar registro"
                    >
                      <Trash2 className="w-4 h-4" />
                    </button>
                  </td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      {/* Paginación */}
      <div className="p-4 border-t border-slate-800 flex items-center justify-between text-xs text-slate-400">
        <div>
          Mostrando página <span className="font-semibold text-white">{page}</span> de <span className="font-semibold text-white">{totalPages}</span> ({totalRecords} mediciones totales)
        </div>
        <div className="flex space-x-2">
          <button
            onClick={() => setPage((p) => Math.max(1, p - 1))}
            disabled={page === 1}
            className="px-3 py-1.5 rounded-lg bg-slate-800 hover:bg-slate-700 disabled:opacity-40 text-white transition-colors"
          >
            Anterior
          </button>
          <button
            onClick={() => setPage((p) => Math.min(totalPages, p + 1))}
            disabled={page === totalPages}
            className="px-3 py-1.5 rounded-lg bg-slate-800 hover:bg-slate-700 disabled:opacity-40 text-white transition-colors"
          >
            Siguiente
          </button>
        </div>
      </div>
    </div>
  );
}

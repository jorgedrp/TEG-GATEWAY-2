import React, { useState, useEffect } from 'react';
import Navbar from './components/Navbar';
import ConfigPanel from './components/ConfigPanel';
import LiveTerminal from './components/LiveTerminal';
import RegistryTable from './components/RegistryTable';
import AnalyticsExplorer from './components/AnalyticsExplorer';
import SyncModal from './components/SyncModal';
import { api } from './services/api';

export default function App() {
  const [activeTab, setActiveTab] = useState('config');
  const [health, setHealth] = useState({ influxdb_connected: false });
  const [processStatus, setProcessStatus] = useState({ running: false });
  const [syncModalOpen, setSyncModalOpen] = useState(false);
  const [targetDeviceForSync, setTargetDeviceForSync] = useState('16');
  const [explorerParams, setExplorerParams] = useState(null);

  // Leer parámetros de URL iniciales
  useEffect(() => {
    const params = new URLSearchParams(window.location.search);
    const path = window.location.pathname;

    if (path.includes('registro') || params.get('tab') === 'registro') {
      setActiveTab('registro');
    } else if (path.includes('explorar') || params.get('tab') === 'explorar') {
      setActiveTab('explorar');
      if (params.get('sensor') || params.get('inicio')) {
        setExplorerParams({
          sensor: params.get('sensor') || '16',
          inicio: params.get('inicio'),
          fin: params.get('fin')
        });
      }
    }
  }, []);

  // Comprobar estado de salud y procesos
  useEffect(() => {
    const checkSystem = async () => {
      try {
        const [h, s] = await Promise.all([
          api.getHealth(),
          api.getProcessStatus()
        ]);
        setHealth(h);
        setProcessStatus(s);
      } catch {
        // Fallback
      }
    };

    checkSystem();
    const interval = setInterval(checkSystem, 3000);
    return () => clearInterval(interval);
  }, []);

  const handleStopProcess = async () => {
    await api.stopProcess();
    const status = await api.getProcessStatus();
    setProcessStatus(status);
  };

  const handleOpenSync = (device) => {
    setTargetDeviceForSync(device);
    setSyncModalOpen(true);
  };

  const handleExecuteSync = async (tipoSync) => {
    await api.sendCommand('sync', {
      device: targetDeviceForSync,
      tipo: tipoSync
    });
  };

  const handleExploreRecord = (record) => {
    setExplorerParams({
      sensor: String(record.node_id),
      inicio: String(record.start_timestamp_ms),
      fin: String(record.stop_timestamp_ms)
    });
    setActiveTab('explorar');
  };

  return (
    <div className="min-h-screen flex flex-col bg-slate-950 text-slate-100 font-sans">
      {/* Navbar Superior */}
      <Navbar
        activeTab={activeTab}
        setActiveTab={setActiveTab}
        health={health}
        processStatus={processStatus}
        onStopProcess={handleStopProcess}
      />

      {/* Contenido Principal */}
      <main className="flex-1 max-w-7xl w-full mx-auto px-4 sm:px-6 lg:px-8 py-8">
        {activeTab === 'config' && (
          <div className="space-y-6">
            <ConfigPanel
              onOpenSync={handleOpenSync}
              onCommandSent={() => api.getProcessStatus().then(setProcessStatus)}
            />
            <LiveTerminal onClearLogs={api.clearLogs} />
          </div>
        )}

        {activeTab === 'registro' && (
          <RegistryTable onExploreRecord={handleExploreRecord} />
        )}

        {activeTab === 'explorar' && (
          <AnalyticsExplorer initialParams={explorerParams} />
        )}
      </main>

      {/* Footer */}
      <footer className="border-t border-slate-900 bg-slate-950 py-4 text-center text-xs text-slate-500">
        <div className="max-w-7xl mx-auto px-4 flex flex-col sm:flex-row items-center justify-between gap-2">
          <span>TEG-GATEWAY • Red Inalámbrica de Monitoreo de Salud Estructural (WSN-SHM)</span>
          <span>Universidad Central de Venezuela (UCV) • Facultad de Ingeniería</span>
        </div>
      </footer>

      {/* Modal de Sincronización */}
      <SyncModal
        isOpen={syncModalOpen}
        onClose={() => setSyncModalOpen(false)}
        onSync={handleExecuteSync}
        targetDevice={targetDeviceForSync}
      />
    </div>
  );
}

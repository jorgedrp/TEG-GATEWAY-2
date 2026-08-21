const API_BASE = '/api';

export const api = {
  // Nodos y Estados
  async getNodes() {
    const res = await fetch(`${API_BASE}/nodes`);
    if (!res.ok) throw new Error('Error al obtener nodos');
    return res.json();
  },

  async updateNode(nodeId, config) {
    const res = await fetch(`${API_BASE}/nodes/${nodeId}`, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(config)
    });
    if (!res.ok) throw new Error('Error al actualizar configuración del nodo');
    return res.json();
  },

  async getNodeStates() {
    const res = await fetch(`${API_BASE}/nodes/states`);
    if (!res.ok) throw new Error('Error al obtener estados de sensores');
    return res.json();
  },

  async detectNodes() {
    const res = await fetch(`${API_BASE}/nodes/detect`, { method: 'POST' });
    return res.json();
  },

  // Orquestador LoRa
  async sendCommand(commandName, payload = {}) {
    const res = await fetch(`${API_BASE}/orchestrator/command/${commandName}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    const data = await res.json();
    if (!res.ok) throw new Error(data.message || data.error || 'Error al ejecutar comando');
    return data;
  },

  async stopProcess() {
    const res = await fetch(`${API_BASE}/orchestrator/stop`, { method: 'POST' });
    return res.json();
  },

  async getProcessStatus() {
    const res = await fetch(`${API_BASE}/orchestrator/status`);
    if (!res.ok) return { running: false };
    return res.json();
  },

  async clearLogs() {
    const res = await fetch(`${API_BASE}/orchestrator/clear-logs`, { method: 'POST' });
    return res.json();
  },

  // Registro de Mediciones
  async getRegistry(page = 1, limit = 50, node = null) {
    let url = `${API_BASE}/registry?page=${page}&limit=${limit}`;
    if (node && node !== '255') url += `&node=${node}`;
    const res = await fetch(url);
    if (!res.ok) throw new Error('Error al cargar registros');
    return res.json();
  },

  async deleteRegistry(id) {
    const res = await fetch(`${API_BASE}/registry/${id}`, { method: 'DELETE' });
    return res.json();
  },

  // Telemetría & DSP
  async getTelemetryData(start, stop, channel, sensor) {
    const params = new URLSearchParams({
      start: start || '-5m',
      stop: stop || 'now()',
      channel: channel || 'ax',
      sensor: sensor || '16'
    });
    const res = await fetch(`${API_BASE}/telemetry/data?${params.toString()}`);
    const data = await res.json();
    if (!res.ok) throw new Error(data.error || 'Error al obtener telemetría');
    return data;
  },

  async getInclinacionData(start, stop, sensor) {
    const params = new URLSearchParams({
      start: start || '-5m',
      stop: stop || 'now()',
      sensor: sensor || '16'
    });
    const res = await fetch(`${API_BASE}/telemetry/inclinacion?${params.toString()}`);
    const data = await res.json();
    if (!res.ok) throw new Error(data.error || 'Error al obtener datos de inclinación');
    return data;
  },

  async getHealth() {
    try {
      const res = await fetch(`${API_BASE}/telemetry/health`);
      return await res.json();
    } catch {
      return { influxdb_connected: false };
    }
  }
};

import React, { useEffect, useRef, useState } from 'react';
import { Terminal, Trash2, Copy, Check, ArrowDown, Shield } from 'lucide-react';

export default function LiveTerminal({ onClearLogs }) {
  const [logs, setLogs] = useState([]);
  const [autoScroll, setAutoScroll] = useState(true);
  const [copied, setCopied] = useState(false);
  const terminalEndRef = useRef(null);

  useEffect(() => {
    // Iniciar conexión SSE
    const eventSource = new EventSource('/api/orchestrator/stream');

    eventSource.onmessage = (event) => {
      if (event.data) {
        const timestamp = new Date().toLocaleTimeString();
        setLogs((prev) => [...prev.slice(-300), { text: event.data, time: timestamp }]);
      }
    };

    eventSource.onerror = () => {
      // Reintentos automáticos del navegador
    };

    return () => {
      eventSource.close();
    };
  }, []);

  useEffect(() => {
    if (autoScroll) {
      terminalEndRef.current?.scrollIntoView({ behavior: 'smooth' });
    }
  }, [logs, autoScroll]);

  const handleCopy = () => {
    const textToCopy = logs.map((l) => `[${l.time}] ${l.text}`).join('\n');
    navigator.clipboard.writeText(textToCopy);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const handleClear = () => {
    setLogs([]);
    if (onClearLogs) onClearLogs();
  };

  const formatLogLine = (line) => {
    if (line.startsWith('[STATUS]') || line.includes('STATUS:')) {
      return <span className="text-emerald-400 font-semibold">{line}</span>;
    }
    if (line.startsWith('[DATA]') || line.includes('DATA:')) {
      return <span className="text-cyan-400 font-semibold">{line}</span>;
    }
    if (line.startsWith('[ORCHESTRATOR]')) {
      return <span className="text-amber-400 font-semibold">{line}</span>;
    }
    if (line.toLowerCase().includes('error') || line.toLowerCase().includes('falló')) {
      return <span className="text-rose-400 font-semibold">{line}</span>;
    }
    if (line.toLowerCase().includes('standby')) {
      return <span className="text-blue-400">{line}</span>;
    }
    if (line.toLowerCase().includes('evento')) {
      return <span className="text-emerald-300">{line}</span>;
    }
    if (line.toLowerCase().includes('tiempo')) {
      return <span className="text-rose-300">{line}</span>;
    }
    return <span className="text-slate-300">{line}</span>;
  };

  return (
    <div className="bg-slate-900 border border-slate-800 rounded-2xl overflow-hidden flex flex-col h-[480px] shadow-xl">
      {/* Header */}
      <div className="bg-slate-950 px-4 py-3 border-b border-slate-800 flex items-center justify-between">
        <div className="flex items-center space-x-2">
          <Terminal className="w-4 h-4 text-emerald-400" />
          <span className="text-sm font-semibold text-white">Monitor de Operaciones LoRa</span>
          <span className="text-[10px] font-mono px-2 py-0.5 rounded-full bg-slate-800 text-slate-400">
            {logs.length} líneas
          </span>
        </div>

        <div className="flex items-center space-x-2">
          <button
            onClick={() => setAutoScroll(!autoScroll)}
            className={`flex items-center space-x-1 px-2.5 py-1 rounded-md text-xs transition-colors ${
              autoScroll ? 'bg-emerald-500/10 text-emerald-400 border border-emerald-500/30' : 'text-slate-400 hover:text-white'
            }`}
            title="Desplazamiento automático"
          >
            <ArrowDown className="w-3.5 h-3.5" />
            <span className="hidden sm:inline">Auto-scroll</span>
          </button>

          <button
            onClick={handleCopy}
            className="p-1.5 text-slate-400 hover:text-white rounded-md hover:bg-slate-800 transition-colors"
            title="Copiar logs al portapapeles"
          >
            {copied ? <Check className="w-4 h-4 text-emerald-400" /> : <Copy className="w-4 h-4" />}
          </button>

          <button
            onClick={handleClear}
            className="p-1.5 text-slate-400 hover:text-rose-400 rounded-md hover:bg-slate-800 transition-colors"
            title="Limpiar consola"
          >
            <Trash2 className="w-4 h-4" />
          </button>
        </div>
      </div>

      {/* Console Stream Feed */}
      <div className="flex-1 p-4 overflow-y-auto font-mono text-xs space-y-1 bg-slate-950/70">
        {logs.length === 0 ? (
          <div className="h-full flex items-center justify-center text-slate-600 italic">
            Esperando eventos u órdenes del orquestador...
          </div>
        ) : (
          logs.map((log, index) => (
            <div key={index} className="flex items-start space-x-2 leading-relaxed hover:bg-slate-900/50 px-1 py-0.5 rounded">
              <span className="text-slate-600 select-none flex-shrink-0">{log.time}</span>
              <span className="text-slate-700 select-none">›</span>
              <div className="flex-1 break-all">{formatLogLine(log.text)}</div>
            </div>
          ))
        )}
        <div ref={terminalEndRef} />
      </div>
    </div>
  );
}

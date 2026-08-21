import subprocess
import threading
import time
import logging
from typing import Optional, Dict, Any, Tuple
from pathlib import Path
from webapp.config import Config
from webapp.app.database.models import NodeRepository, MeasurementRepository
from webapp.app.services.sse_service import sse_broadcaster

logger = logging.getLogger(__name__)

class ProcessManager:
    """Administrador seguro y centralizado para la ejecución de subprocesos en C del Gateway."""

    def __init__(self):
        self._lock = threading.RLock()
        self._active_process: Optional[subprocess.Popen] = None
        self._current_command: Optional[str] = None
        self._started_at: Optional[float] = None

    def stop_active_process(self, timeout_sec: float = 2.0) -> bool:
        """Detiene de forma limpia el subproceso activo."""
        with self._lock:
            if not self._active_process or self._active_process.poll() is not None:
                self._active_process = None
                self._current_command = None
                self._started_at = None
                return True

            logger.info(f"Deteniendo subproceso activo (PID: {self._active_process.pid})...")
            try:
                self._active_process.terminate()
                try:
                    self._active_process.wait(timeout=timeout_sec)
                except subprocess.TimeoutExpired:
                    logger.warning("El subproceso no finalizó a tiempo con SIGTERM; forzando SIGKILL.")
                    self._active_process.kill()
                    self._active_process.wait(timeout=1.0)
            except Exception as e:
                logger.error(f"Error al detener subproceso: {e}")
            finally:
                self._active_process = None
                self._current_command = None
                self._started_at = None

            return True

    def get_status(self) -> Dict[str, Any]:
        """Devuelve el estado actual del orquestador."""
        with self._lock:
            is_running = self._active_process is not None and self._active_process.poll() is None
            return {
                "running": is_running,
                "command": self._current_command if is_running else None,
                "pid": self._active_process.pid if is_running else None,
                "uptime_seconds": round(time.time() - self._started_at, 1) if is_running and self._started_at else 0
            }

    def start_command(self, command_name: str, payload: Dict[str, Any]) -> Tuple[bool, str]:
        """Prepara y ejecuta el binario C correspondiente al comando solicitado."""
        with self._lock:
            # 1. Detener proceso previo
            self.stop_active_process()
            sse_broadcaster.clear_history()

            device = str(payload.get('device', '255'))
            frecuencia = str(payload.get('frecuencia', '5'))
            lora = str(payload.get('lora', '3'))
            ventana = str(payload.get('ventana', '35'))
            umbral = str(payload.get('umbral', '0.2'))
            tipo_sync = str(payload.get('tipo', 'simple'))

            bin_dir = Config.GATEWAY_BIN_DIR
            c_env = Config.get_env_dict_for_c()

            # Obtener configuraciones actuales de los nodos en SQLite
            nodes_data = {n['node_id']: n for n in NodeRepository.get_all()}

            cmd_args = []
            msg = ""

            try:
                if command_name == 'standby':
                    bin_path = bin_dir / 'standby_mode'
                    if device == '255':
                        cmd_args = [str(bin_path), '255', '16', '32', '48', '64']
                        msg = "Iniciando modo Standby en todos los sensores."
                    else:
                        cmd_args = [str(bin_path), device]
                        msg = f"Iniciando modo Standby en el sensor {device}."

                elif command_name == 'eventos':
                    bin_path = bin_dir / 'event_mode'
                    if device == '255':
                        n16 = nodes_data.get('16', {})
                        n32 = nodes_data.get('32', {})
                        n48 = nodes_data.get('48', {})
                        n64 = nodes_data.get('64', {})
                        cmd_args = [
                            str(bin_path), '255',
                            '16', str(n16.get('threshold', '0.2')), str(n16.get('lora_mode', '3')), str(n16.get('frequency', '5')), str(n16.get('window_time', '35')),
                            '32', str(n32.get('threshold', '0.2')), str(n32.get('lora_mode', '3')), str(n32.get('frequency', '5')), str(n32.get('window_time', '35')),
                            '48', str(n48.get('threshold', '0.2')), str(n48.get('lora_mode', '3')), str(n48.get('frequency', '5')), str(n48.get('window_time', '35')),
                            '64', str(n64.get('threshold', '0.2')), str(n64.get('lora_mode', '3')), str(n64.get('frequency', '5')), str(n64.get('window_time', '35'))
                        ]
                        msg = "Iniciando modo Eventos en todos los sensores."
                    else:
                        dev_cfg = nodes_data.get(device, {})
                        cmd_args = [
                            str(bin_path), device,
                            str(dev_cfg.get('threshold', umbral)),
                            str(dev_cfg.get('lora_mode', lora)),
                            str(dev_cfg.get('frequency', frecuencia)),
                            str(dev_cfg.get('window_time', ventana))
                        ]
                        msg = f"Iniciando modo Eventos en el sensor {device}."

                elif command_name == 'tiempo':
                    bin_path = bin_dir / 'time_mode'
                    if device == '255':
                        n16 = nodes_data.get('16', {})
                        n32 = nodes_data.get('32', {})
                        n48 = nodes_data.get('48', {})
                        n64 = nodes_data.get('64', {})
                        cmd_args = [
                            str(bin_path), '255',
                            '16', str(n16.get('lora_mode', '3')), str(n16.get('frequency', '5')), str(n16.get('window_time', '35')),
                            '32', str(n32.get('lora_mode', '3')), str(n32.get('frequency', '5')), str(n32.get('window_time', '35')),
                            '48', str(n48.get('lora_mode', '3')), str(n48.get('frequency', '5')), str(n48.get('window_time', '35')),
                            '64', str(n64.get('lora_mode', '3')), str(n64.get('frequency', '5')), str(n64.get('window_time', '35'))
                        ]
                        msg = "Iniciando modo Tiempo en todos los sensores."
                    else:
                        dev_cfg = nodes_data.get(device, {})
                        cmd_args = [
                            str(bin_path), device,
                            str(dev_cfg.get('lora_mode', lora)),
                            str(dev_cfg.get('frequency', frecuencia)),
                            str(dev_cfg.get('window_time', ventana))
                        ]
                        msg = f"Iniciando modo Tiempo en el sensor {device}."

                elif command_name == 'configurar':
                    # Guardar parámetros primero en SQLite
                    NodeRepository.upsert_config(device, frecuencia, lora, ventana, umbral)
                    bin_path = bin_dir / 'config_mode'
                    cmd_args = [str(bin_path), device, frecuencia, lora]
                    msg = f"Enviando configuración OTA al sensor {device}."

                elif command_name == 'sync':
                    sync_code = '2' if tipo_sync in ('completa', '2') else '1'
                    bin_path = bin_dir / 'clock'
                    cmd_args = [str(bin_path), device, sync_code]
                    msg = f"Iniciando sincronización de reloj ({'Completa' if sync_code == '2' else 'Simple'}) con el sensor {device}."

                elif command_name == 'detectar':
                    bin_path = bin_dir / 'detect_mode'
                    cmd_args = [str(bin_path)]
                    msg = "Iniciando descubrimiento de sensores en la red LoRa."

                else:
                    return False, f"Comando '{command_name}' no reconocido."

                # Validar existencia del binario
                target_bin = Path(cmd_args[0])
                if not target_bin.exists():
                    logger.warning(f"Binario C no encontrado en {target_bin}. Verifique la compilación.")
                    return False, f"Binario no encontrado: {target_bin.name}. Ejecute 'make build-gateway' primero."

                logger.info(f"Ejecutando comando: {' '.join(cmd_args)}")
                self._active_process = subprocess.Popen(
                    cmd_args,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    env=c_env,
                    bufsize=1
                )
                self._current_command = command_name
                self._started_at = time.time()

                # Lanzar hilo de monitoreo de salida
                monitor_thread = threading.Thread(
                    target=self._monitor_output,
                    args=(self._active_process, command_name),
                    daemon=True
                )
                monitor_thread.start()

                sse_broadcaster.broadcast(f"[ORCHESTRATOR] {msg}")
                return True, msg

            except Exception as e:
                logger.error(f"Error al iniciar subproceso: {e}")
                return False, f"Error al ejecutar comando: {str(e)}"

    def _monitor_output(self, process: subprocess.Popen, cmd_name: str):
        """Lee el stdout del subproceso en C, parsea eventos y actualiza la base de datos y SSE."""
        try:
            for line in iter(process.stdout.readline, ''):
                line = line.strip()
                if not line:
                    continue

                # 1. Evento de cambio o verificación de estado
                if line.startswith("STATUS:"):
                    parts = line.split(":")
                    if len(parts) >= 3:
                        sensor_id = parts[1].strip()
                        mode = parts[2].strip().upper()
                        NodeRepository.update_mode(sensor_id, mode)
                        sse_broadcaster.broadcast(f"[STATUS] Sensor {sensor_id} -> {mode}")

                # 2. Evento de registro de medición finalizada
                elif line.startswith("DATA:"):
                    parts = line.split(":")
                    if len(parts) >= 4:
                        sensor_id = parts[1].strip()
                        try:
                            start_ms = int(parts[2].strip())
                            stop_ms = int(parts[3].strip())
                            rec_id = MeasurementRepository.add(sensor_id, start_ms, stop_ms, record_type='EVENTO')
                            sse_broadcaster.broadcast(f"[DATA] Registro #{rec_id} guardado para sensor {sensor_id}")
                        except ValueError:
                            pass

                # 3. Línea de log estándar
                else:
                    sse_broadcaster.broadcast(line)

        except Exception as e:
            logger.error(f"Error en monitor de salida C: {e}")
        finally:
            process.stdout.close()
            process.wait()
            logger.info(f"Subproceso '{cmd_name}' finalizó con código {process.returncode}")
            sse_broadcaster.broadcast(f"[ORCHESTRATOR] Proceso '{cmd_name}' terminado (código {process.returncode})")
            sse_broadcaster.broadcast_end()

# Singleton global del gestor de procesos
process_manager = ProcessManager()

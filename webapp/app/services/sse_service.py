import queue
import threading
from collections import deque
from typing import Generator, List

class SSEBroadcaster:
    """Gestor centralizado de suscripción y transmisión Server-Sent Events (SSE)."""

    def __init__(self, max_history: int = 250):
        self._lock = threading.Lock()
        self._listeners: List[queue.Queue] = []
        self._history = deque(maxlen=max_history)

    def broadcast(self, message: str):
        """Envía un mensaje a todos los clientes web conectados y lo almacena en el historial."""
        if not message:
            return

        with self._lock:
            self._history.append(message)
            dead_listeners = []
            for q in self._listeners:
                try:
                    q.put_nowait(message)
                except queue.Full:
                    dead_listeners.append(q)

            for dead in dead_listeners:
                if dead in self._listeners:
                    self._listeners.remove(dead)

    def broadcast_end(self):
        """Notifica el fin de la ejecución de un subproceso enviando señal de cierre."""
        with self._lock:
            for q in self._listeners:
                try:
                    q.put_nowait(None)
                except Exception:
                    pass

    def clear_history(self):
        """Limpia el buffer de historial de logs."""
        with self._lock:
            self._history.clear()

    def get_history(self) -> List[str]:
        """Obtiene una copia del historial actual de logs."""
        with self._lock:
            return list(self._history)

    def subscribe(self) -> Generator[str, None, None]:
        """Generador SSE para un cliente conectado individual."""
        client_queue = queue.Queue(maxsize=500)

        with self._lock:
            # Enviar primero los logs históricos recientes
            for past_line in self._history:
                client_queue.put_nowait(past_line)
            self._listeners.append(client_queue)

        try:
            while True:
                msg = client_queue.get()
                if msg is None:
                    # Señal de fin de proceso (opcional: cerrar o mantener abierto el stream)
                    yield "event: end\ndata: [Proceso C finalizado]\n\n"
                    continue
                yield f"data: {msg}\n\n"
        except GeneratorExit:
            pass
        finally:
            with self._lock:
                if client_queue in self._listeners:
                    self._listeners.remove(client_queue)

# Singleton global para la aplicación
sse_broadcaster = SSEBroadcaster()

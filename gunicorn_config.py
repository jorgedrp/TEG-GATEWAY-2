import multiprocessing
import os

# Configuración del servidor WSGI Gunicorn para TEG-GATEWAY en Raspberry Pi
bind = f"{os.getenv('FLASK_HOST', '0.0.0.0')}:{os.getenv('FLASK_PORT', 5000)}"

# En Raspberry Pi se recomienda 2-4 threads por worker para manejar conexiones concurrentes y SSE
workers = 1  # 1 worker para mantener el singleton del orquestador LoRa y los hilos en el mismo proceso
threads = 8  # 8 hilos para manejar múltiples clientes SSE y peticiones REST concurrentes
worker_class = 'gthread'

timeout = 120
keepalive = 5
max_requests = 1000
max_requests_jitter = 50

# Logging
accesslog = '-'
errorlog = '-'
loglevel = os.getenv('LOG_LEVEL', 'info').lower()

# Process naming
proc_name = 'teg-gateway'

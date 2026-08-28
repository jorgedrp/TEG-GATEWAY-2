# ==============================================================================
# Multi-stage Dockerfile para TEG-GATEWAY
# ==============================================================================

# ------------------------------------------------------------------------------
# ETAPA 1: Construcción del Frontend React (Node.js)
# ------------------------------------------------------------------------------
FROM node:20-bookworm-slim AS frontend-builder
WORKDIR /build

# Copiar dependencias de frontend y construir artefactos de producción
COPY webapp/frontend/package*.json ./
RUN npm install

COPY webapp/frontend/ ./
RUN npm run build

# ------------------------------------------------------------------------------
# ETAPA 2: Entorno de Ejecución (Python + Compilador C + Librerías de Hardware)
# ------------------------------------------------------------------------------
FROM python:3.11-slim-bookworm AS runtime

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    DEBIAN_FRONTEND=noninteractive \
    GATEWAY_BIN_DIR=/app/gateway \
    DATABASE_PATH=/app/webapp/gateway.db

WORKDIR /app

# 1. Instalar dependencias de compilación y librerías C de hardware (SPI/GPIO/cURL)
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc \
    make \
    pkg-config \
    libgpiod-dev \
    libgpiod2 \
    libcurl4-openssl-dev \
    libcurl4 \
    curl \
    && rm -rf /var/lib/apt/lists/*

# 2. Instalar dependencias de Python
COPY requirements.txt .
RUN pip install --no-cache-dir --upgrade pip && \
    pip install --no-cache-dir -r requirements.txt

# 3. Copiar código fuente de módulos C del Gateway y compilar binarios
COPY gateway/ ./gateway/
RUN make -C gateway all

# 4. Copiar código de la aplicación web y configuración
COPY webapp/ ./webapp/
COPY wsgi.py gunicorn_config.py ./

# 5. Copiar frontend compilado desde la Etapa 1
COPY --from=frontend-builder /app/static/dist ./webapp/app/static/dist

# 6. Crear directorio para base de datos SQLite persistente
RUN mkdir -p /app/data && mkdir -p /app/webapp

EXPOSE 5000

# Comando por defecto: Servidor WSGI Gunicorn en producción
CMD ["gunicorn", "-c", "gunicorn_config.py", "wsgi:app"]

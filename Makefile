VENV ?= .venv
PYTHON ?= $(VENV)/bin/python3
PIP ?= $(VENV)/bin/pip
GUNICORN ?= $(VENV)/bin/gunicorn
PYTEST ?= $(VENV)/bin/pytest

.PHONY: help build build-gateway build-frontend setup-backend test test-spi run run-prod docker-build docker-up docker-down docker-logs clean

help:
	@echo "TEG-GATEWAY Orchestration Makefile"
	@echo "----------------------------------"
	@echo "  make build          - Build C binaries and React frontend"
	@echo "  make build-gateway  - Compile C LoRa/SPI binaries"
	@echo "  make build-frontend - Install & build React/Vite web interface"
	@echo "  make setup-backend  - Create virtualenv & install Python dependencies"
	@echo "  make test           - Run automated test suite with pytest"
	@echo "  make test-spi       - Run hardware SPI LoRa diagnostics"
	@echo "  make run            - Run backend in development mode"
	@echo "  make run-prod       - Run backend with Gunicorn"
	@echo "  make docker-build   - Build Docker container image"
	@echo "  make docker-up      - Start full Docker Compose stack (Gateway + InfluxDB)"
	@echo "  make docker-down    - Stop Docker Compose stack"
	@echo "  make docker-logs    - Follow live Docker container logs"
	@echo "  make clean          - Remove build artifacts and temporary files"

build: build-gateway build-frontend

build-gateway:
	@echo "==> Compilando binarios C del gateway..."
	$(MAKE) -C gateway all

test-spi: build-gateway
	@echo "==> Ejecutando diagnóstico de hardware SPI LoRa..."
	./gateway/test_spi

build-frontend:
	@echo "==> Construyendo frontend React / Vite..."
	cd webapp/frontend && npm install && npm run build

setup-backend:
	@echo "==> Configurando entorno virtual e instalando dependencias..."
	@if [ ! -f "$(PYTHON)" ]; then \
		rm -rf $(VENV); \
		python3 -m venv $(VENV) || { echo "Error: ejecuta 'sudo apt install -y python3-venv python3-pip' primero"; exit 1; }; \
	fi
	$(PYTHON) -m pip install --upgrade pip
	$(PIP) install -r requirements.txt

test:
	@echo "==> Ejecutando suite de pruebas..."
	PYTHONPATH=. $(PYTEST) tests/ -v

run:
	@echo "==> Iniciando servidor Flask (Dev)..."
	PYTHONPATH=. $(PYTHON) wsgi.py

run-prod:
	@echo "==> Iniciando servidor Gunicorn (Prod)..."
	$(GUNICORN) -c gunicorn_config.py wsgi:app

docker-build:
	@echo "==> Construyendo imagen Docker de TEG-GATEWAY..."
	docker compose build

docker-up:
	@echo "==> Levantando stack completo en Docker (Gateway + InfluxDB)..."
	docker compose up -d

docker-down:
	@echo "==> Deteniendo contenedores Docker..."
	docker compose down

docker-logs:
	@echo "==> Mostrando logs en tiempo real..."
	docker compose logs -f

clean:
	$(MAKE) -C gateway clean
	rm -rf webapp/frontend/dist webapp/frontend/node_modules
	find . -type d -name "__pycache__" -exec rm -rf {} +
	find . -type f -name "*.pyc" -delete

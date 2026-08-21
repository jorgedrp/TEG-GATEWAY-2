VENV ?= .venv
PYTHON ?= $(VENV)/bin/python3
PIP ?= $(VENV)/bin/pip
GUNICORN ?= $(VENV)/bin/gunicorn
PYTEST ?= $(VENV)/bin/pytest

.PHONY: help build build-gateway build-frontend setup-backend test run run-prod clean

help:
	@echo "TEG-GATEWAY Orchestration Makefile"
	@echo "----------------------------------"
	@echo "  make build          - Build C binaries and React frontend"
	@echo "  make build-gateway  - Compile C LoRa/SPI binaries"
	@echo "  make build-frontend - Install & build React/Vite web interface"
	@echo "  make setup-backend  - Create virtualenv & install Python dependencies"
	@echo "  make test           - Run automated test suite with pytest"
	@echo "  make run            - Run backend in development mode"
	@echo "  make run-prod       - Run backend with Gunicorn"
	@echo "  make clean          - Remove build artifacts and temporary files"

build: build-gateway build-frontend

build-gateway:
	@echo "==> Compilando binarios C del gateway..."
	$(MAKE) -C gateway all

build-frontend:
	@echo "==> Construyendo frontend React / Vite..."
	cd webapp/frontend && npm install && npm run build

setup-backend:
	@echo "==> Configurando entorno virtual e instalando dependencias..."
	@if [ ! -d "$(VENV)" ]; then \
		python3 -m venv $(VENV) || { echo "Error: ejecuta 'sudo apt install python3-venv' primero"; exit 1; }; \
	fi
	$(PIP) install --upgrade pip
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

clean:
	$(MAKE) -C gateway clean
	rm -rf webapp/frontend/dist webapp/frontend/node_modules
	find . -type d -name "__pycache__" -exec rm -rf {} +
	find . -type f -name "*.pyc" -delete

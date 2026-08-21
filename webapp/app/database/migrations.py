import json
import csv
import logging
from pathlib import Path
from webapp.config import Config
from webapp.app.database.connection import get_db
from webapp.app.database.models import SCHEMA_SQL, NodeRepository, MeasurementRepository

logger = logging.getLogger(__name__)

def init_db(db_path=None):
    """Inicializa el esquema de base de datos e importa datos existentes si es necesario."""
    path = db_path or Config.DATABASE_PATH
    logger.info(f"Inicializando base de datos SQLite en: {path}")

    # 1. Crear tablas e índices
    with get_db(path) as conn:
        conn.executescript(SCHEMA_SQL)

    # 2. Migrar o inicializar nodos desde config.json o valores por defecto
    existing_nodes = NodeRepository.get_all(path)
    if not existing_nodes:
        _migrate_config_json(path)
    
    # 3. Migrar registros históricos desde datalog.csv si la tabla measurements está vacía
    if MeasurementRepository.count(db_path=path) == 0:
        _migrate_datalog_csv(path)

def _migrate_config_json(db_path):
    config_file = Config.LEGACY_CONFIG_PATH
    default_nodes = {
        "16": {"frequency": "5", "lora": "3", "ventana": "35", "umbral": "0.2", "mode": "OFF"},
        "32": {"frequency": "5", "lora": "3", "ventana": "35", "umbral": "0.2", "mode": "OFF"},
        "48": {"frequency": "5", "lora": "3", "ventana": "35", "umbral": "0.2", "mode": "OFF"},
        "64": {"frequency": "5", "lora": "3", "ventana": "35", "umbral": "0.2", "mode": "OFF"},
    }

    loaded_data = {}
    if config_file.exists():
        try:
            with open(config_file, 'r', encoding='utf-8') as f:
                loaded_data = json.load(f)
            logger.info("Migrando configuraciones previas desde config.json a SQLite...")
        except Exception as e:
            logger.warning(f"No se pudo leer {config_file} para migración: {e}")

    for node_id in ["16", "32", "48", "64"]:
        node_raw = loaded_data.get(node_id, {})
        cfg = node_raw.get("config", {})
        modes = node_raw.get("mode", {})

        freq = str(cfg.get("frecuency", default_nodes[node_id]["frequency"]))
        lora = str(cfg.get("lora", default_nodes[node_id]["lora"]))
        win = str(cfg.get("ventana", default_nodes[node_id]["ventana"]))
        umb = str(cfg.get("umbral", default_nodes[node_id]["umbral"]))

        mode = "OFF"
        if modes.get("standby") == 1:
            mode = "STANDBY"
        elif modes.get("event") == 1:
            mode = "EVENTO"
        elif modes.get("tiempo") == 1:
            mode = "TIEMPO"
        elif modes.get("clock") == 1:
            mode = "CLOCK"

        NodeRepository.upsert_config(node_id, freq, lora, win, umb, db_path=db_path)
        NodeRepository.update_mode(node_id, mode, db_path=db_path)

def _migrate_datalog_csv(db_path):
    csv_file = Config.LEGACY_DATALOG_PATH
    if not csv_file.exists():
        return

    logger.info("Migrando historial previo desde datalog.csv a SQLite...")
    try:
        count = 0
        with open(csv_file, mode='r', encoding='utf-8') as f:
            reader = csv.reader(f)
            for row in reader:
                if not row or len(row) < 3:
                    continue
                node_id = str(row[0]).strip()
                try:
                    start_ms = int(row[1].strip())
                    stop_ms = int(row[2].strip())
                    MeasurementRepository.add(node_id, start_ms, stop_ms, record_type='EVENTO', db_path=db_path)
                    count += 1
                except ValueError:
                    continue
        logger.info(f"Migrados {count} registros históricos a SQLite.")
    except Exception as e:
        logger.warning(f"Error migrando datalog.csv: {e}")

import sqlite3
from typing import List, Dict, Any, Optional
from webapp.app.database.connection import get_db

SCHEMA_SQL = """
CREATE TABLE IF NOT EXISTS nodes (
    node_id TEXT PRIMARY KEY,
    frequency TEXT NOT NULL DEFAULT '5',
    lora_mode TEXT NOT NULL DEFAULT '3',
    window_time TEXT NOT NULL DEFAULT '35',
    threshold TEXT NOT NULL DEFAULT '0.2',
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS node_states (
    node_id TEXT PRIMARY KEY,
    current_mode TEXT NOT NULL DEFAULT 'OFF',
    last_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (node_id) REFERENCES nodes(node_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS measurements (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    node_id TEXT NOT NULL,
    start_timestamp_ms INTEGER NOT NULL,
    stop_timestamp_ms INTEGER NOT NULL,
    record_type TEXT DEFAULT 'EVENTO',
    notes TEXT DEFAULT '',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_measurements_node ON measurements(node_id);
CREATE INDEX IF NOT EXISTS idx_measurements_timestamps ON measurements(start_timestamp_ms, stop_timestamp_ms);
"""

class NodeRepository:
    """Acceso a datos para configuración de sensores y estados."""

    @staticmethod
    def get_all(db_path=None) -> List[Dict[str, Any]]:
        with get_db(db_path) as conn:
            cursor = conn.cursor()
            query = """
            SELECT n.node_id, n.frequency, n.lora_mode, n.window_time, n.threshold, n.updated_at,
                   COALESCE(s.current_mode, 'OFF') as current_mode,
                   s.last_seen
            FROM nodes n
            LEFT JOIN node_states s ON n.node_id = s.node_id
            ORDER BY CAST(n.node_id AS INTEGER) ASC;
            """
            cursor.execute(query)
            return [dict(row) for row in cursor.fetchall()]

    @staticmethod
    def get_by_id(node_id: str, db_path=None) -> Optional[Dict[str, Any]]:
        with get_db(db_path) as conn:
            cursor = conn.cursor()
            query = """
            SELECT n.node_id, n.frequency, n.lora_mode, n.window_time, n.threshold, n.updated_at,
                   COALESCE(s.current_mode, 'OFF') as current_mode,
                   s.last_seen
            FROM nodes n
            LEFT JOIN node_states s ON n.node_id = s.node_id
            WHERE n.node_id = ?;
            """
            cursor.execute(query, (str(node_id),))
            row = cursor.fetchone()
            return dict(row) if row else None

    @staticmethod
    def upsert_config(node_id: str, frequency: str, lora_mode: str, window_time: str, threshold: str, db_path=None):
        with get_db(db_path) as conn:
            cursor = conn.cursor()
            query = """
            INSERT INTO nodes (node_id, frequency, lora_mode, window_time, threshold, updated_at)
            VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
            ON CONFLICT(node_id) DO UPDATE SET
                frequency = excluded.frequency,
                lora_mode = excluded.lora_mode,
                window_time = excluded.window_time,
                threshold = excluded.threshold,
                updated_at = CURRENT_TIMESTAMP;
            """
            cursor.execute(query, (str(node_id), str(frequency), str(lora_mode), str(window_time), str(threshold)))

            # Asegurar que existe registro de estado
            cursor.execute("""
            INSERT OR IGNORE INTO node_states (node_id, current_mode, last_seen)
            VALUES (?, 'OFF', CURRENT_TIMESTAMP);
            """, (str(node_id),))

    @staticmethod
    def update_mode(node_id: str, mode: str, db_path=None):
        with get_db(db_path) as conn:
            cursor = conn.cursor()
            # Asegurar que el nodo existe
            cursor.execute("""
            INSERT OR IGNORE INTO nodes (node_id) VALUES (?);
            """, (str(node_id),))

            cursor.execute("""
            INSERT INTO node_states (node_id, current_mode, last_seen)
            VALUES (?, ?, CURRENT_TIMESTAMP)
            ON CONFLICT(node_id) DO UPDATE SET
                current_mode = excluded.current_mode,
                last_seen = CURRENT_TIMESTAMP;
            """, (str(node_id), mode.upper()))

    @staticmethod
    def get_all_states(db_path=None) -> Dict[str, str]:
        with get_db(db_path) as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT node_id, current_mode FROM node_states;")
            return {row["node_id"]: row["current_mode"] for row in cursor.fetchall()}

class MeasurementRepository:
    """Acceso a datos para el registro de sesiones y mediciones de eventos."""

    @staticmethod
    def add(node_id: str, start_ms: int, stop_ms: int, record_type: str = 'EVENTO', notes: str = '', db_path=None) -> int:
        with get_db(db_path) as conn:
            cursor = conn.cursor()
            query = """
            INSERT INTO measurements (node_id, start_timestamp_ms, stop_timestamp_ms, record_type, notes, created_at)
            VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP);
            """
            cursor.execute(query, (str(node_id), int(start_ms), int(stop_ms), record_type, notes))
            return cursor.lastrowid

    @staticmethod
    def get_all(limit: int = 200, offset: int = 0, node_id: Optional[str] = None, db_path=None) -> List[Dict[str, Any]]:
        with get_db(db_path) as conn:
            cursor = conn.cursor()
            if node_id and node_id != '255':
                query = """
                SELECT id, node_id, start_timestamp_ms, stop_timestamp_ms, record_type, notes, created_at,
                       ((stop_timestamp_ms - start_timestamp_ms) / 1000.0) as duration_seconds
                FROM measurements
                WHERE node_id = ?
                ORDER BY id DESC
                LIMIT ? OFFSET ?;
                """
                cursor.execute(query, (str(node_id), limit, offset))
            else:
                query = """
                SELECT id, node_id, start_timestamp_ms, stop_timestamp_ms, record_type, notes, created_at,
                       ((stop_timestamp_ms - start_timestamp_ms) / 1000.0) as duration_seconds
                FROM measurements
                ORDER BY id DESC
                LIMIT ? OFFSET ?;
                """
                cursor.execute(query, (limit, offset))
            return [dict(row) for row in cursor.fetchall()]

    @staticmethod
    def count(node_id: Optional[str] = None, db_path=None) -> int:
        with get_db(db_path) as conn:
            cursor = conn.cursor()
            if node_id and node_id != '255':
                cursor.execute("SELECT COUNT(*) FROM measurements WHERE node_id = ?;", (str(node_id),))
            else:
                cursor.execute("SELECT COUNT(*) FROM measurements;")
            return cursor.fetchone()[0]

    @staticmethod
    def delete(measurement_id: int, db_path=None) -> bool:
        with get_db(db_path) as conn:
            cursor = conn.cursor()
            cursor.execute("DELETE FROM measurements WHERE id = ?;", (measurement_id,))
            return cursor.rowcount > 0

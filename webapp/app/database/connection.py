import sqlite3
from contextlib import contextmanager
from flask import has_app_context, current_app
from webapp.config import Config

def get_connection(db_path=None):
    """Crea una conexión SQLite con modo WAL y timeout configurado para concurrencia."""
    if db_path is None:
        if has_app_context() and current_app.config.get('DATABASE_PATH'):
            path = current_app.config['DATABASE_PATH']
        else:
            path = Config.DATABASE_PATH
    else:
        path = db_path

    conn = sqlite3.connect(str(path), timeout=10.0)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode = WAL;")
    conn.execute("PRAGMA synchronous = NORMAL;")
    conn.execute("PRAGMA foreign_keys = ON;")
    conn.execute("PRAGMA busy_timeout = 5000;")
    return conn

@contextmanager
def get_db(db_path=None):
    """Context manager para operaciones transaccionales seguras."""
    conn = get_connection(db_path)
    try:
        yield conn
        conn.commit()
    except Exception:
        conn.rollback()
        raise
    finally:
        conn.close()

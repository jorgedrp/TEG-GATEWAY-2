import os
from pathlib import Path
from zoneinfo import ZoneInfo
from dotenv import load_dotenv

# Localizar la raíz del proyecto para cargar .env
BASE_DIR = Path(__file__).resolve().parent.parent
dotenv_path = BASE_DIR / '.env'
if dotenv_path.exists():
    load_dotenv(dotenv_path)
else:
    load_dotenv()

class Config:
    """Configuración centralizada de la aplicación."""
    
    # Entorno y Servidor Flask
    ENV = os.getenv('FLASK_ENV', 'production')
    DEBUG = os.getenv('FLASK_DEBUG', 'False').lower() in ('true', '1', 't')
    PORT = int(os.getenv('FLASK_PORT', 5000))
    HOST = os.getenv('FLASK_HOST', '0.0.0.0')
    SECRET_KEY = os.getenv('SECRET_KEY', 'teg-gateway-secret-key-production-default')

    # Configuración de InfluxDB v2
    INFLUX_URL = os.getenv('INFLUXDB_URL', 'http://localhost:8086')
    INFLUX_TOKEN = os.getenv('INFLUXDB_TOKEN', '')
    INFLUX_ORG = os.getenv('INFLUXDB_ORG', 'UCV')
    INFLUX_BUCKET = os.getenv('INFLUXDB_BUCKET', 'lora_table')

    # Directorio de binarios C del Gateway
    _raw_gateway_bin = os.getenv('GATEWAY_BIN_DIR', str(BASE_DIR / 'gateway'))
    GATEWAY_BIN_DIR = Path(_raw_gateway_bin) if os.path.isabs(_raw_gateway_bin) else BASE_DIR / _raw_gateway_bin

    # Base de Datos SQLite
    _raw_db_path = os.getenv('DATABASE_PATH', 'gateway.db')
    DATABASE_PATH = Path(_raw_db_path) if os.path.isabs(_raw_db_path) else BASE_DIR / 'webapp' / _raw_db_path

    # Archivos Legados para Migración
    LEGACY_CONFIG_PATH = BASE_DIR / 'webapp' / 'config.json'
    LEGACY_DATALOG_PATH = BASE_DIR / 'webapp' / 'datalog.csv'

    # Zona Horaria
    TIMEZONE_NAME = os.getenv('TIMEZONE', 'America/Caracas')
    try:
        TIMEZONE = ZoneInfo(TIMEZONE_NAME)
    except Exception:
        TIMEZONE = ZoneInfo('UTC')

    # Nivel de Logging
    LOG_LEVEL = os.getenv('LOG_LEVEL', 'INFO').upper()

    @classmethod
    def get_env_dict_for_c(cls) -> dict:
        """Devuelve un diccionario de variables de entorno para pasar a subprocesos C."""
        env = os.environ.copy()
        env['INFLUXDB_URL'] = cls.INFLUX_URL
        env['INFLUXDB_TOKEN'] = cls.INFLUX_TOKEN
        env['INFLUXDB_ORG'] = cls.INFLUX_ORG
        env['INFLUXDB_BUCKET'] = cls.INFLUX_BUCKET
        return env

import os
import logging
from pathlib import Path
from flask import Flask
from webapp.config import Config
from webapp.app.database.migrations import init_db

def create_app(config_class=Config):
    """Application Factory para TEG-GATEWAY."""
    base_dir = Path(__file__).resolve().parent.parent
    static_dir = base_dir / 'app' / 'static'
    template_dir = base_dir / 'templates'

    # Configuración de Logging
    logging.basicConfig(
        level=getattr(logging, config_class.LOG_LEVEL, logging.INFO),
        format='%(asctime)s [%(levelname)s] %(name)s: %(message)s'
    )
    logger = logging.getLogger(__name__)

    app = Flask(
        __name__,
        static_folder=str(static_dir),
        template_folder=str(template_dir)
    )
    app.config.from_object(config_class)

    # Inicializar CORS si está disponible
    try:
        from flask_cors import CORS
        CORS(app)
    except ImportError:
        pass

    # Inicializar Base de Datos SQLite
    try:
        init_db(config_class.DATABASE_PATH)
    except Exception as e:
        logger.error(f"Error inicializando la base de datos: {e}")

    # Registrar Blueprints
    from webapp.app.blueprints import (
        nodes_bp,
        orchestrator_bp,
        telemetry_bp,
        registry_bp,
        legacy_bp,
        views_bp
    )

    app.register_blueprint(nodes_bp)
    app.register_blueprint(orchestrator_bp)
    app.register_blueprint(telemetry_bp)
    app.register_blueprint(registry_bp)
    app.register_blueprint(legacy_bp)
    app.register_blueprint(views_bp)

    logger.info("Aplicación Flask TEG-GATEWAY inicializada correctamente.")
    return app

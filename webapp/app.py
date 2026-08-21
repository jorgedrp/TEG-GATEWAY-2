#!/usr/bin/env python3
"""
Punto de entrada compatible para TEG-GATEWAY.
Permite ejecutar 'python3 app.py' directamente desde la carpeta webapp/.
"""
import sys
from pathlib import Path

# Asegurar que el root del proyecto esté en el sys.path
root_dir = Path(__file__).resolve().parent.parent
if str(root_dir) not in sys.path:
    sys.path.insert(0, str(root_dir))

from webapp.app import create_app
from webapp.config import Config

app = create_app(Config)

if __name__ == '__main__':
    app.run(
        host=Config.HOST,
        port=Config.PORT,
        debug=Config.DEBUG,
        threaded=True
    )

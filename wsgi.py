import sys
from pathlib import Path

# Añadir el directorio raíz al path de Python
root_path = Path(__file__).resolve().parent
if str(root_path) not in sys.path:
    sys.path.insert(0, str(root_path))

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

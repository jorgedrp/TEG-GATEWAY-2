from pathlib import Path
from flask import Blueprint, send_from_directory, current_app, render_template

views_bp = Blueprint('views', __name__)

@views_bp.route('/', defaults={'path': ''})
@views_bp.route('/<path:path>')
def serve_spa(path):
    """Sirve la aplicación de frontend SPA o los templates de fallback."""
    dist_dir = Path(current_app.static_folder) / 'dist'
    
    # Si existe el build de React en static/dist/, servirlo como SPA
    if (dist_dir / 'index.html').exists():
        if path != "" and (dist_dir / path).exists():
            return send_from_directory(dist_dir, path)
        return send_from_directory(dist_dir, 'index.html')

    # Fallback a templates tradicionales si el frontend no está compilado aún
    if path == 'registro':
        return render_template('registro.html')
    elif path == 'explorar':
        return render_template('explorar.html')
    return render_template('index.html')

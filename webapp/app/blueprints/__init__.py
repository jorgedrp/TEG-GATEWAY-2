from webapp.app.blueprints.nodes import nodes_bp
from webapp.app.blueprints.orchestrator import orchestrator_bp
from webapp.app.blueprints.telemetry import telemetry_bp
from webapp.app.blueprints.registry import registry_bp
from webapp.app.blueprints.legacy import legacy_bp
from webapp.app.blueprints.views import views_bp

__all__ = [
    'nodes_bp',
    'orchestrator_bp',
    'telemetry_bp',
    'registry_bp',
    'legacy_bp',
    'views_bp'
]

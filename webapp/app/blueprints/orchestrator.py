from flask import Blueprint, jsonify, request, Response
from webapp.app.services.process_manager import process_manager
from webapp.app.services.sse_service import sse_broadcaster

orchestrator_bp = Blueprint('orchestrator', __name__, url_prefix='/api/orchestrator')

@orchestrator_bp.route('/command/<string:command_name>', methods=['POST'])
def execute_command(command_name):
    """Ejecuta un comando operativo del gateway LoRa."""
    payload = request.get_json() or {}
    success, message = process_manager.start_command(command_name, payload)
    status_code = 200 if success else 400
    return jsonify({"success": success, "message": message}), status_code

@orchestrator_bp.route('/stop', methods=['POST'])
def stop_process():
    """Detiene cualquier proceso activo del orquestador."""
    stopped = process_manager.stop_active_process()
    sse_broadcaster.broadcast("[ORCHESTRATOR] Proceso detenido por el usuario.")
    return jsonify({"success": stopped, "message": "Proceso detenido con éxito."})

@orchestrator_bp.route('/status', methods=['GET'])
def get_status():
    """Devuelve el estado de ejecución del orquestador."""
    status = process_manager.get_status()
    return jsonify(status)

@orchestrator_bp.route('/stream', methods=['GET'])
def sse_stream():
    """Endpoint Server-Sent Events (SSE) para recepción de logs en tiempo real."""
    return Response(
        sse_broadcaster.subscribe(),
        mimetype='text/event-stream',
        headers={
            'Cache-Control': 'no-cache',
            'X-Accel-Buffering': 'no',
            'Connection': 'keep-alive'
        }
    )

@orchestrator_bp.route('/clear-logs', methods=['POST'])
def clear_logs():
    """Limpia el buffer de logs en memoria."""
    sse_broadcaster.clear_history()
    return jsonify({"success": True, "message": "Historial de logs limpiado."})

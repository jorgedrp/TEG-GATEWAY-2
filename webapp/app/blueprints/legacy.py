from flask import Blueprint, jsonify, request, Response
from webapp.app.services.process_manager import process_manager
from webapp.app.services.sse_service import sse_broadcaster
from webapp.app.services.influx_service import influx_service
from webapp.app.database.models import NodeRepository

legacy_bp = Blueprint('legacy', __name__)

@legacy_bp.route('/api/command/<string:command_name>', methods=['POST'])
def legacy_handle_command(command_name):
    """Compatibilidad: Ejecuta comandos orquestadores."""
    payload = request.get_json() or {}
    success, message = process_manager.start_command(command_name, payload)
    status_code = 200 if success else 400
    return jsonify({"message": message}), status_code

@legacy_bp.route('/api/detectar', methods=['POST'])
def legacy_detectar():
    """Compatibilidad: Devuelve el estado actual de los sensores."""
    states = NodeRepository.get_all_states()
    # Garantizar que todos los IDs base (16, 32, 48, 64) estén presentes
    for default_id in ["16", "32", "48", "64"]:
        if default_id not in states:
            states[default_id] = "OFF"
    return jsonify(states)

@legacy_bp.route('/api/stream', methods=['GET'])
def legacy_stream():
    """Compatibilidad: Retransmite logs por SSE."""
    return Response(
        sse_broadcaster.subscribe(),
        mimetype='text/event-stream',
        headers={
            'Cache-Control': 'no-cache',
            'X-Accel-Buffering': 'no',
            'Connection': 'keep-alive'
        }
    )

@legacy_bp.route('/api/data', methods=['GET'])
def legacy_get_chart_data():
    """Compatibilidad: Consulta series temporales y FFT."""
    start_time = request.args.get('start', '-5m')
    stop_time = request.args.get('stop', 'now()')
    channel = request.args.get('channel', 'ax')
    sensor = request.args.get('sensor', '16')

    try:
        data = influx_service.query_telemetry(start_time, stop_time, channel, sensor)
        return jsonify(data)
    except ValueError as ve:
        return jsonify({"error": str(ve)}), 400
    except LookupError as le:
        return jsonify({"error": str(le)}), 404
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@legacy_bp.route('/api/inclinacion', methods=['GET'])
def legacy_get_incl_data():
    """Compatibilidad: Consulta inclinación Roll/Pitch con filtro de Kalman."""
    start_time = request.args.get('start', '-5m')
    stop_time = request.args.get('stop', 'now()')
    sensor = request.args.get('sensor', '16')

    try:
        data = influx_service.query_inclinacion(start_time, stop_time, sensor)
        return jsonify(data)
    except ValueError as ve:
        return jsonify({"error": str(ve)}), 400
    except LookupError as le:
        return jsonify({"error": str(le)}), 404
    except Exception as e:
        return jsonify({"error": str(e)}), 500

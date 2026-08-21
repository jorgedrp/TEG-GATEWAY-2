from flask import Blueprint, jsonify, request
from webapp.app.services.influx_service import influx_service

telemetry_bp = Blueprint('telemetry', __name__, url_prefix='/api/telemetry')

@telemetry_bp.route('/data', methods=['GET'])
def get_telemetry_data():
    """Consulta series de tiempo de aceleración/giroscopio y variables ambientales con FFT."""
    start_time = request.args.get('start', '-5m')
    stop_time = request.args.get('stop', 'now()')
    channel = request.args.get('channel', 'ax')
    sensor = request.args.get('sensor', '16')

    try:
        data = influx_service.query_telemetry(
            start_time=start_time,
            stop_time=stop_time,
            channel=channel,
            sensor_id=sensor
        )
        return jsonify(data)
    except ValueError as ve:
        return jsonify({"error": str(ve)}), 400
    except LookupError as le:
        return jsonify({"error": str(le)}), 404
    except Exception as e:
        return jsonify({"error": f"Error interno en consulta de telemetría: {str(e)}"}), 500

@telemetry_bp.route('/inclinacion', methods=['GET'])
def get_inclinacion_data():
    """Consulta mediciones 6-DoF y calcula inclinación estructural Roll/Pitch con Filtro de Kalman."""
    start_time = request.args.get('start', '-5m')
    stop_time = request.args.get('stop', 'now()')
    sensor = request.args.get('sensor', '16')

    try:
        data = influx_service.query_inclinacion(
            start_time=start_time,
            stop_time=stop_time,
            sensor_id=sensor
        )
        return jsonify(data)
    except ValueError as ve:
        return jsonify({"error": str(ve)}), 400
    except LookupError as le:
        return jsonify({"error": str(le)}), 404
    except Exception as e:
        return jsonify({"error": f"Error interno en cálculo de inclinación: {str(e)}"}), 500

@telemetry_bp.route('/health', methods=['GET'])
def get_health():
    """Comprueba el estado de la conexión con InfluxDB."""
    connected = influx_service.is_connected()
    return jsonify({
        "influxdb_connected": connected,
        "url": influx_service.url,
        "bucket": influx_service.bucket
    }), 200 if connected else 503

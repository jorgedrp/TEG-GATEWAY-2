import io
import csv
from datetime import datetime
from flask import Blueprint, jsonify, request, Response
from webapp.config import Config
from webapp.app.database.models import MeasurementRepository

registry_bp = Blueprint('registry', __name__, url_prefix='/api/registry')

def _format_record(row: dict) -> dict:
    """Añade fechas legibles formateadas a la zona horaria configurada."""
    start_ms = row.get('start_timestamp_ms', 0)
    stop_ms = row.get('stop_timestamp_ms', 0)

    try:
        dt_start = datetime.fromtimestamp(start_ms / 1000.0, tz=Config.TIMEZONE)
        inicio_legible = dt_start.strftime('%Y-%m-%d %H:%M:%S')
    except Exception:
        inicio_legible = str(start_ms)

    try:
        dt_stop = datetime.fromtimestamp(stop_ms / 1000.0, tz=Config.TIMEZONE)
        fin_legible = dt_stop.strftime('%Y-%m-%d %H:%M:%S')
    except Exception:
        fin_legible = str(stop_ms)

    return {
        "id": row.get('id'),
        "node_id": row.get('node_id'),
        "start_timestamp_ms": start_ms,
        "stop_timestamp_ms": stop_ms,
        "inicio_legible": inicio_legible,
        "fin_legible": fin_legible,
        "duration_seconds": round(float(row.get('duration_seconds') or ((stop_ms - start_ms) / 1000.0)), 2),
        "record_type": row.get('record_type', 'EVENTO'),
        "notes": row.get('notes', ''),
        "created_at": row.get('created_at')
    }

@registry_bp.route('', methods=['GET'])
def list_records():
    """Devuelve la lista paginada de registros históricos de medición."""
    limit = int(request.args.get('limit', 100))
    page = max(1, int(request.args.get('page', 1)))
    node_id = request.args.get('node', None)

    offset = (page - 1) * limit
    raw_records = MeasurementRepository.get_all(limit=limit, offset=offset, node_id=node_id)
    total = MeasurementRepository.count(node_id=node_id)

    formatted = [_format_record(r) for r in raw_records]
    return jsonify({
        "records": formatted,
        "total": total,
        "page": page,
        "limit": limit,
        "total_pages": max(1, (total + limit - 1) // limit)
    })

@registry_bp.route('', methods=['POST'])
def add_record():
    """Añade un nuevo registro de medición a la base de datos."""
    data = request.get_json() or {}
    node_id = str(data.get('node_id', '16'))
    try:
        start_ms = int(data.get('start_timestamp_ms', 0))
        stop_ms = int(data.get('stop_timestamp_ms', 0))
    except (ValueError, TypeError):
        return jsonify({"error": "Timestamps deben ser enteros en milisegundos."}), 400

    record_type = data.get('record_type', 'EVENTO')
    notes = data.get('notes', '')

    new_id = MeasurementRepository.add(node_id, start_ms, stop_ms, record_type, notes)
    return jsonify({"success": True, "id": new_id, "message": "Registro creado con éxito."}), 201

@registry_bp.route('/<int:record_id>', methods=['DELETE'])
def delete_record(record_id):
    """Elimina un registro de medición."""
    deleted = MeasurementRepository.delete(record_id)
    if not deleted:
        return jsonify({"error": f"Registro con ID {record_id} no encontrado."}), 404
    return jsonify({"success": True, "message": f"Registro {record_id} eliminado."})

@registry_bp.route('/export', methods=['GET'])
def export_csv():
    """Exporta todo el registro histórico a un archivo CSV descargable."""
    node_id = request.args.get('node', None)
    records = MeasurementRepository.get_all(limit=10000, offset=0, node_id=node_id)

    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow(['ID', 'Sensor', 'Inicio_ms', 'Fin_ms', 'Inicio_Legible', 'Fin_Legible', 'Duracion_Seg', 'Tipo', 'Notas'])

    for r in records:
        fmt = _format_record(r)
        writer.writerow([
            fmt['id'], fmt['node_id'], fmt['start_timestamp_ms'], fmt['stop_timestamp_ms'],
            fmt['inicio_legible'], fmt['fin_legible'], fmt['duration_seconds'],
            fmt['record_type'], fmt['notes']
        ])

    csv_data = output.getvalue()
    return Response(
        csv_data,
        mimetype='text/csv',
        headers={'Content-Disposition': f'attachment; filename=teg_registry_{datetime.now().strftime("%Y%m%d_%H%M%S")}.csv'}
    )

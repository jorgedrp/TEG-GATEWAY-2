from flask import Blueprint, jsonify, request
from webapp.app.database.models import NodeRepository
from webapp.app.services.process_manager import process_manager

nodes_bp = Blueprint('nodes', __name__, url_prefix='/api/nodes')

@nodes_bp.route('', methods=['GET'])
def list_nodes():
    """Devuelve la lista de nodos configurados y su estado actual."""
    nodes = NodeRepository.get_all()
    return jsonify({"nodes": nodes})

@nodes_bp.route('/<string:node_id>', methods=['GET'])
def get_node(node_id):
    """Devuelve la configuración y estado de un nodo específico."""
    node = NodeRepository.get_by_id(node_id)
    if not node:
        return jsonify({"error": f"Nodo con ID '{node_id}' no encontrado."}), 404
    return jsonify(node)

@nodes_bp.route('/<string:node_id>', methods=['PUT', 'POST'])
def update_node(node_id):
    """Actualiza los parámetros de configuración de un sensor."""
    data = request.get_json() or {}
    freq = str(data.get('frequency', data.get('frecuencia', '5')))
    lora = str(data.get('lora_mode', data.get('lora', '3')))
    win = str(data.get('window_time', data.get('ventana', '35')))
    umb = str(data.get('threshold', data.get('umbral', '0.2')))

    NodeRepository.upsert_config(node_id, freq, lora, win, umb)
    updated = NodeRepository.get_by_id(node_id)
    return jsonify({"message": f"Configuración de nodo {node_id} actualizada.", "node": updated})

@nodes_bp.route('/states', methods=['GET'])
def get_states():
    """Devuelve el mapa de estados actuales de todos los sensores."""
    states = NodeRepository.get_all_states()
    return jsonify(states)

@nodes_bp.route('/detect', methods=['POST'])
def trigger_detect():
    """Dispara la rutina de descubrimiento de nodos en la red LoRa."""
    success, msg = process_manager.start_command('detectar', {})
    status_code = 200 if success else 400
    return jsonify({"success": success, "message": msg}), status_code

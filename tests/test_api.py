import pytest
from webapp.app import create_app
from webapp.config import Config

class TestConfig(Config):
    TESTING = True
    DATABASE_PATH = ":memory:"

@pytest.fixture
def client(tmp_path):
    test_db = tmp_path / "api_test.db"
    
    class RuntimeTestConfig(Config):
        TESTING = True
        DATABASE_PATH = test_db
        DEBUG = False

    app = create_app(RuntimeTestConfig)
    with app.test_client() as client:
        yield client

def test_nodes_api(client):
    response = client.get('/api/nodes')
    assert response.status_code == 200
    json_data = response.get_json()
    assert "nodes" in json_data
    assert len(json_data["nodes"]) >= 4

def test_nodes_states_api(client):
    response = client.get('/api/nodes/states')
    assert response.status_code == 200
    json_data = response.get_json()
    assert "16" in json_data

def test_registry_api(client):
    # GET list
    response = client.get('/api/registry')
    assert response.status_code == 200
    json_data = response.get_json()
    assert "records" in json_data

    # POST new record
    post_res = client.post('/api/registry', json={
        "node_id": "32",
        "start_timestamp_ms": 1700000000000,
        "stop_timestamp_ms": 1700000030000,
        "record_type": "TIEMPO"
    })
    assert post_res.status_code == 201

def test_orchestrator_status_api(client):
    response = client.get('/api/orchestrator/status')
    assert response.status_code == 200
    json_data = response.get_json()
    assert "running" in json_data
    assert json_data["running"] is False

def test_legacy_detectar_endpoint(client):
    response = client.post('/api/detectar')
    assert response.status_code == 200
    json_data = response.get_json()
    assert "16" in json_data

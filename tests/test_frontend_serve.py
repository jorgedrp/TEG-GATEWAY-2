import pytest
from webapp.app import create_app
from webapp.config import Config

@pytest.fixture
def client(tmp_path):
    test_db = tmp_path / "frontend_test.db"
    
    class RuntimeTestConfig(Config):
        TESTING = True
        DATABASE_PATH = test_db
        DEBUG = False

    app = create_app(RuntimeTestConfig)
    with app.test_client() as client:
        yield client

def test_serve_spa_index(client):
    response = client.get('/')
    assert response.status_code == 200
    assert b"TEG-GATEWAY" in response.data or b"html" in response.data

def test_serve_spa_routes(client):
    res_registro = client.get('/registro')
    assert res_registro.status_code == 200

    res_explorar = client.get('/explorar')
    assert res_explorar.status_code == 200

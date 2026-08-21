import pytest
from pathlib import Path
from webapp.app.database.migrations import init_db
from webapp.app.database.models import NodeRepository, MeasurementRepository

@pytest.fixture
def temp_db(tmp_path):
    db_file = tmp_path / "test_gateway.db"
    init_db(db_file)
    return db_file

def test_node_repository_crud(temp_db):
    # Upsert node config
    NodeRepository.upsert_config("16", frequency="10", lora_mode="2", window_time="50", threshold="0.45", db_path=temp_db)
    
    node = NodeRepository.get_by_id("16", db_path=temp_db)
    assert node is not None
    assert node["frequency"] == "10"
    assert node["lora_mode"] == "2"
    assert node["window_time"] == "50"
    assert node["threshold"] == "0.45"

    # Update mode
    NodeRepository.update_mode("16", "STANDBY", db_path=temp_db)
    states = NodeRepository.get_all_states(db_path=temp_db)
    assert states.get("16") == "STANDBY"

def test_measurement_repository_crud(temp_db):
    # Add measurement
    rec_id = MeasurementRepository.add("16", start_ms=1700000000000, stop_ms=1700000050000, record_type="EVENTO", notes="Prueba unitaria", db_path=temp_db)
    assert rec_id > 0

    count = MeasurementRepository.count(db_path=temp_db)
    assert count >= 1

    records = MeasurementRepository.get_all(limit=10, db_path=temp_db)
    assert len(records) >= 1
    assert records[0]["node_id"] == "16"

    # Delete measurement
    deleted = MeasurementRepository.delete(rec_id, db_path=temp_db)
    assert deleted is True

"""
API integration tests using FastAPI TestClient.
"""

import pytest
from fastapi.testclient import TestClient

import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from rigidlabeler_backend.api.server import app


@pytest.fixture
def client():
    """Create a test client."""
    return TestClient(app)


class TestHealthEndpoint:
    """Tests for /health endpoint."""
    
    def test_health_check(self, client):
        """Test health check returns OK."""
        response = client.get("/health")
        assert response.status_code == 200
        
        data = response.json()
        assert data["status"] == "ok"
        assert "data" in data
        assert data["data"]["backend"] == "fastapi"


class TestComputeRigidEndpoint:
    """Tests for /compute/rigid endpoint."""
    
    def test_compute_rigid_success(self, client):
        """Test successful rigid transform computation."""
        request_data = {
            "tie_points": [
                {"fixed": {"x": 0, "y": 0}, "moving": {"x": 10, "y": 10}},
                {"fixed": {"x": 100, "y": 0}, "moving": {"x": 110, "y": 10}},
                {"fixed": {"x": 100, "y": 100}, "moving": {"x": 110, "y": 110}},
                {"fixed": {"x": 0, "y": 100}, "moving": {"x": 10, "y": 110}}
            ],
            "allow_scale": False,
            "min_points_required": 2
        }
        
        response = client.post("/compute/rigid", json=request_data)
        assert response.status_code == 200
        
        data = response.json()
        assert data["status"] == "ok"
        assert "data" in data
        assert "rigid" in data["data"]
        assert "matrix_3x3" in data["data"]
        assert "rms_error" in data["data"]
    
    def test_compute_rigid_not_enough_points(self, client):
        """Test error when not enough points provided."""
        request_data = {
            "tie_points": [
                {"fixed": {"x": 0, "y": 0}, "moving": {"x": 10, "y": 10}}
            ],
            "allow_scale": False,
            "min_points_required": 2
        }
        
        response = client.post("/compute/rigid", json=request_data)
        assert response.status_code == 200
        
        data = response.json()
        assert data["status"] == "error"
        assert data["error_code"] == "NOT_ENOUGH_POINTS"


class TestLabelsEndpoints:
    """Tests for /labels/* endpoints."""
    
    def test_labels_list_empty(self, client):
        """Test listing labels (may be empty)."""
        response = client.get("/labels/list")
        assert response.status_code == 200
        
        data = response.json()
        assert data["status"] == "ok"
        assert isinstance(data["data"], list)
    
    def test_load_nonexistent_label(self, client):
        """Test loading a non-existent label."""
        response = client.get(
            "/labels/load",
            params={
                "image_fixed": "nonexistent/fixed.png",
                "image_moving": "nonexistent/moving.png"
            }
        )
        assert response.status_code == 200
        
        data = response.json()
        assert data["status"] == "error"
        assert data["error_code"] == "LABEL_NOT_FOUND"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])

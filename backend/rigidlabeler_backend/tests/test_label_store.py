"""
Unit tests for label storage.
"""

import pytest
import tempfile
import json
from pathlib import Path

import sys
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from rigidlabeler_backend.api.schemas import (
    Label, TiePoint, Point2D, RigidParams, LabelMeta
)
from rigidlabeler_backend.io.label_store import (
    save_label, load_label, list_labels, delete_label,
    generate_label_id, generate_label_filename,
    LabelStoreError
)


class TestLabelIdGeneration:
    """Tests for label ID and filename generation."""
    
    def test_consistent_id(self):
        """Same inputs should produce same ID."""
        id1 = generate_label_id("path/to/fixed.png", "path/to/moving.png")
        id2 = generate_label_id("path/to/fixed.png", "path/to/moving.png")
        assert id1 == id2
    
    def test_different_id(self):
        """Different inputs should produce different IDs."""
        id1 = generate_label_id("path/to/fixed1.png", "path/to/moving.png")
        id2 = generate_label_id("path/to/fixed2.png", "path/to/moving.png")
        assert id1 != id2
    
    def test_filename_format(self):
        """Filename should follow expected format."""
        filename = generate_label_filename("vis_001.png", "ir_001.png")
        assert filename.endswith(".json")
        assert "vis_001" in filename
        assert "ir_001" in filename


class TestLabelStorage:
    """Tests for label save/load operations."""
    
    def create_test_label(self) -> Label:
        """Create a test label for testing."""
        return Label(
            image_fixed="test/fixed.png",
            image_moving="test/moving.png",
            rigid=RigidParams(
                theta_deg=5.0,
                tx=10.0,
                ty=-5.0,
                scale=1.0
            ),
            matrix_3x3=[
                [0.996, -0.087, 10.0],
                [0.087, 0.996, -5.0],
                [0.0, 0.0, 1.0]
            ],
            tie_points=[
                TiePoint(
                    fixed=Point2D(x=100.0, y=100.0),
                    moving=Point2D(x=90.0, y=105.0)
                ),
                TiePoint(
                    fixed=Point2D(x=200.0, y=150.0),
                    moving=Point2D(x=190.0, y=155.0)
                )
            ],
            meta=LabelMeta(comment="test label")
        )
    
    def test_save_and_load(self):
        """Test saving and loading a label."""
        with tempfile.TemporaryDirectory() as tmpdir:
            label = self.create_test_label()
            
            # Save
            result = save_label(label, labels_root=tmpdir)
            assert result.label_id
            assert Path(result.label_path).exists()
            
            # Load
            loaded = load_label(
                label.image_fixed,
                label.image_moving,
                labels_root=tmpdir
            )
            
            assert loaded.image_fixed == label.image_fixed
            assert loaded.image_moving == label.image_moving
            assert loaded.rigid.theta_deg == label.rigid.theta_deg
            assert loaded.rigid.tx == label.rigid.tx
            assert loaded.rigid.ty == label.rigid.ty
            assert len(loaded.tie_points) == len(label.tie_points)
    
    def test_load_not_found(self):
        """Test loading non-existent label."""
        with tempfile.TemporaryDirectory() as tmpdir:
            with pytest.raises(LabelStoreError) as exc_info:
                load_label("nonexistent/fixed.png", "nonexistent/moving.png", labels_root=tmpdir)
            
            assert exc_info.value.error_code == "LABEL_NOT_FOUND"
    
    def test_list_labels(self):
        """Test listing labels."""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Save a few labels
            for i in range(3):
                label = Label(
                    image_fixed=f"fixed_{i}.png",
                    image_moving=f"moving_{i}.png",
                    rigid=RigidParams(theta_deg=0, tx=0, ty=0, scale=1.0),
                    matrix_3x3=[[1, 0, 0], [0, 1, 0], [0, 0, 1]],
                    tie_points=[]
                )
                save_label(label, labels_root=tmpdir)
            
            # List
            labels = list_labels(labels_root=tmpdir)
            assert len(labels) == 3
    
    def test_delete_label(self):
        """Test deleting a label."""
        with tempfile.TemporaryDirectory() as tmpdir:
            label = self.create_test_label()
            save_label(label, labels_root=tmpdir)
            
            # Verify it exists
            loaded = load_label(
                label.image_fixed,
                label.image_moving,
                labels_root=tmpdir
            )
            assert loaded is not None
            
            # Delete
            deleted = delete_label(
                label.image_fixed,
                label.image_moving,
                labels_root=tmpdir
            )
            assert deleted
            
            # Verify it's gone
            with pytest.raises(LabelStoreError):
                load_label(
                    label.image_fixed,
                    label.image_moving,
                    labels_root=tmpdir
                )


if __name__ == "__main__":
    pytest.main([__file__, "-v"])

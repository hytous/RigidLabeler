"""
Unit tests for core transform algorithms.
"""

import pytest
import numpy as np
import math

import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent))

from rigidlabeler_backend.core.transforms import (
    compute_rigid_transform,
    transform_points,
    rigid_params_to_matrix,
    matrix_to_rigid_params,
    TransformEstimationError
)


class TestRigidTransform:
    """Tests for rigid transformation estimation."""
    
    def test_identity_transform(self):
        """Points without transformation should yield identity."""
        fixed = np.array([
            [0, 0],
            [100, 0],
            [100, 100],
            [0, 100]
        ], dtype=float)
        moving = fixed.copy()
        
        result = compute_rigid_transform(fixed, moving, allow_scale=False)
        
        assert abs(result.theta_deg) < 1e-6
        assert abs(result.tx) < 1e-6
        assert abs(result.ty) < 1e-6
        assert abs(result.scale - 1.0) < 1e-6
        assert result.rms_error < 1e-6
    
    def test_pure_translation(self):
        """Test pure translation estimation."""
        fixed = np.array([
            [10, 20],
            [110, 20],
            [110, 120],
            [10, 120]
        ], dtype=float)
        # Moving points shifted by (-10, -20)
        moving = fixed - np.array([10, 20])
        
        result = compute_rigid_transform(fixed, moving, allow_scale=False)
        
        assert abs(result.theta_deg) < 1e-6
        assert abs(result.tx - 10) < 1e-6
        assert abs(result.ty - 20) < 1e-6
        assert result.rms_error < 1e-6
    
    def test_pure_rotation(self):
        """Test pure rotation estimation (90 degrees)."""
        fixed = np.array([
            [1, 0],
            [0, 1],
            [-1, 0],
            [0, -1]
        ], dtype=float)
        # Moving points rotated -90 degrees (so we need +90 to match)
        moving = np.array([
            [0, -1],
            [1, 0],
            [0, 1],
            [-1, 0]
        ], dtype=float)
        
        result = compute_rigid_transform(fixed, moving, allow_scale=False)
        
        assert abs(result.theta_deg - 90) < 1e-4
        assert abs(result.tx) < 1e-6
        assert abs(result.ty) < 1e-6
        assert result.rms_error < 1e-6
    
    def test_rotation_and_translation(self):
        """Test combined rotation and translation."""
        theta = 30  # degrees
        tx, ty = 50, -30
        
        # Create moving points
        moving = np.array([
            [0, 0],
            [100, 0],
            [100, 100],
            [0, 100]
        ], dtype=float)
        
        # Apply known transformation to get fixed points
        matrix = rigid_params_to_matrix(theta, tx, ty)
        fixed = transform_points(moving, matrix)
        
        # Estimate transformation
        result = compute_rigid_transform(fixed, moving, allow_scale=False)
        
        assert abs(result.theta_deg - theta) < 1e-4
        assert abs(result.tx - tx) < 1e-4
        assert abs(result.ty - ty) < 1e-4
        assert result.rms_error < 1e-6
    
    def test_with_scale(self):
        """Test similarity transform (with uniform scale)."""
        theta = 45
        tx, ty = 20, 10
        scale = 1.5
        
        moving = np.array([
            [0, 0],
            [100, 0],
            [100, 100],
            [0, 100]
        ], dtype=float)
        
        matrix = rigid_params_to_matrix(theta, tx, ty, scale)
        fixed = transform_points(moving, matrix)
        
        result = compute_rigid_transform(fixed, moving, allow_scale=True)
        
        assert abs(result.theta_deg - theta) < 1e-4
        assert abs(result.tx - tx) < 1e-4
        assert abs(result.ty - ty) < 1e-4
        assert abs(result.scale - scale) < 1e-4
        assert result.rms_error < 1e-6
    
    def test_minimum_points(self):
        """Test with minimum number of points (2)."""
        fixed = np.array([[0, 0], [100, 0]], dtype=float)
        moving = np.array([[10, 10], [110, 10]], dtype=float)
        
        result = compute_rigid_transform(fixed, moving, allow_scale=False)
        
        assert result.num_points == 2
        assert abs(result.tx - (-10)) < 1e-6
        assert abs(result.ty - (-10)) < 1e-6
    
    def test_not_enough_points(self):
        """Test error when not enough points."""
        fixed = np.array([[0, 0]], dtype=float)
        moving = np.array([[10, 10]], dtype=float)
        
        with pytest.raises(TransformEstimationError) as exc_info:
            compute_rigid_transform(fixed, moving)
        
        assert exc_info.value.error_code == "NOT_ENOUGH_POINTS"
    
    def test_mismatched_points(self):
        """Test error when point counts don't match."""
        fixed = np.array([[0, 0], [1, 1]], dtype=float)
        moving = np.array([[0, 0]], dtype=float)
        
        with pytest.raises(TransformEstimationError) as exc_info:
            compute_rigid_transform(fixed, moving)
        
        assert exc_info.value.error_code == "INVALID_INPUT"


class TestMatrixConversion:
    """Tests for matrix conversion functions."""
    
    def test_params_to_matrix_identity(self):
        """Test identity transformation."""
        matrix = rigid_params_to_matrix(0, 0, 0, 1.0)
        expected = np.eye(3)
        np.testing.assert_array_almost_equal(matrix, expected)
    
    def test_params_to_matrix_rotation(self):
        """Test 90 degree rotation."""
        matrix = rigid_params_to_matrix(90, 0, 0, 1.0)
        expected = np.array([
            [0, -1, 0],
            [1, 0, 0],
            [0, 0, 1]
        ])
        np.testing.assert_array_almost_equal(matrix, expected, decimal=10)
    
    def test_matrix_to_params_roundtrip(self):
        """Test roundtrip conversion."""
        original = (45.0, 100.0, -50.0, 1.2)
        matrix = rigid_params_to_matrix(*original)
        recovered = matrix_to_rigid_params(matrix)
        
        assert abs(recovered[0] - original[0]) < 1e-6
        assert abs(recovered[1] - original[1]) < 1e-6
        assert abs(recovered[2] - original[2]) < 1e-6
        assert abs(recovered[3] - original[3]) < 1e-6


class TestTransformPoints:
    """Tests for point transformation."""
    
    def test_identity(self):
        """Test identity transformation."""
        points = np.array([[1, 2], [3, 4], [5, 6]], dtype=float)
        matrix = np.eye(3)
        
        result = transform_points(points, matrix)
        np.testing.assert_array_almost_equal(result, points)
    
    def test_translation(self):
        """Test pure translation."""
        points = np.array([[0, 0], [10, 10]], dtype=float)
        matrix = rigid_params_to_matrix(0, 5, -3, 1.0)
        
        result = transform_points(points, matrix)
        expected = np.array([[5, -3], [15, 7]], dtype=float)
        np.testing.assert_array_almost_equal(result, expected)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])

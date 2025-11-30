"""
2D Rigid Transformation Estimation.

Implements Kabsch/Procrustes analysis using SVD to estimate rigid (or similarity)
transformations between point sets.

The transformation maps points from the moving image to the fixed image:
    p_fixed = scale * R(theta) * p_moving + [tx, ty]

Coordinate convention:
- When using center-origin coordinates (default), points are relative to their
  respective image centers.
- The computed transformation matrix T satisfies: [x'; y'; 1]^T = T @ [x; y; 1]^T
  where (x, y) are moving image coordinates and (x', y') are fixed image coordinates.
"""

import numpy as np
from typing import Tuple, List, Optional
from dataclasses import dataclass
import math


@dataclass
class RigidTransformResult:
    """Result of rigid transformation estimation."""
    theta_deg: float  # Rotation angle in degrees (counter-clockwise)
    tx: float         # Translation in x (column direction)
    ty: float         # Translation in y (row direction)
    scale: float      # Uniform scale factor (1.0 for pure rigid)
    matrix_3x3: np.ndarray  # 3x3 homogeneous transformation matrix
    rms_error: float  # RMS residual error
    num_points: int   # Number of points used


class TransformEstimationError(Exception):
    """Exception raised when transformation estimation fails."""
    def __init__(self, message: str, error_code: str):
        super().__init__(message)
        self.error_code = error_code


def compute_rigid_transform(
    fixed_points: np.ndarray,
    moving_points: np.ndarray,
    allow_scale: bool = False
) -> RigidTransformResult:
    """Estimate 2D rigid (or similarity) transformation using Kabsch algorithm.
    
    Uses centroid alignment + SVD to find the optimal rotation and translation
    that maps moving_points to fixed_points.
    
    The transformation satisfies: p_fixed = s * R * p_moving + t
    
    Args:
        fixed_points: Nx2 array of points in the fixed image (destination).
        moving_points: Nx2 array of corresponding points in the moving image (source).
        allow_scale: If True, also estimate uniform scale (similarity transform).
        
    Returns:
        RigidTransformResult containing the estimated parameters.
        
    Raises:
        TransformEstimationError: If estimation fails (not enough points, singular, etc.)
    """
    # Input validation
    if len(fixed_points) != len(moving_points):
        raise TransformEstimationError(
            "Number of fixed and moving points must match",
            "INVALID_INPUT"
        )
    
    n_points = len(fixed_points)
    if n_points < 2:
        raise TransformEstimationError(
            f"Not enough points to estimate rigid transform (got {n_points}, need at least 2)",
            "NOT_ENOUGH_POINTS"
        )
    
    # Convert to numpy arrays if needed
    src_pts = np.asarray(moving_points, dtype=np.float64)  # Source (moving)
    dst_pts = np.asarray(fixed_points, dtype=np.float64)   # Destination (fixed)
    
    # Check for NaN or Inf
    if not np.all(np.isfinite(src_pts)) or not np.all(np.isfinite(dst_pts)):
        raise TransformEstimationError(
            "Input points contain NaN or Inf values",
            "INVALID_INPUT"
        )
    
    # Step 1: Compute centroids
    mu_src = np.mean(src_pts, axis=0)  # Moving image centroid
    mu_dst = np.mean(dst_pts, axis=0)  # Fixed image centroid
    
    # Step 2: Center the points (remove translation)
    X = src_pts - mu_src  # Centered source points
    Y = dst_pts - mu_dst  # Centered destination points
    
    # Step 3: Compute cross-covariance matrix H = X^T @ Y
    # This follows the Kabsch algorithm convention
    H = X.T @ Y  # 2x2 matrix
    
    # Step 4: SVD of H
    try:
        U, S, Vt = np.linalg.svd(H)
    except np.linalg.LinAlgError:
        raise TransformEstimationError(
            "SVD failed: singular covariance matrix",
            "SINGULAR_TRANSFORM"
        )
    
    # Step 5: Compute rotation matrix R = V @ U^T
    R = Vt.T @ U.T
    
    # Ensure proper rotation (det(R) = +1, not reflection)
    if np.linalg.det(R) < 0:
        # Flip sign of last row of Vt (equivalent to last column of V)
        Vt[1, :] *= -1
        R = Vt.T @ U.T
    
    # Step 6: Compute scale (if requested)
    if allow_scale:
        # Compute scale using Umeyama's formula
        var_src = np.sum(X ** 2)
        if var_src < 1e-12:
            raise TransformEstimationError(
                "Source points have zero variance",
                "SINGULAR_TRANSFORM"
            )
        scale = np.sum(S) / var_src
    else:
        scale = 1.0
    
    # Step 7: Compute translation
    # t = mu_dst - scale * R @ mu_src
    t = mu_dst - scale * (R @ mu_src)
    
    # Step 8: Extract rotation angle
    theta_rad = np.arctan2(R[1, 0], R[0, 0])
    theta_deg = np.degrees(theta_rad)
    
    # Step 9: Build 3x3 homogeneous transformation matrix
    matrix_3x3 = np.eye(3)
    matrix_3x3[0:2, 0:2] = scale * R
    matrix_3x3[0:2, 2] = t
    
    # Step 10: Compute RMS error
    # Transform moving points and compute residuals
    transformed_src = transform_points(src_pts, matrix_3x3)
    residuals = dst_pts - transformed_src
    rms_error = np.sqrt(np.mean(np.sum(residuals ** 2, axis=1)))
    
    return RigidTransformResult(
        theta_deg=float(theta_deg),
        tx=float(t[0]),
        ty=float(t[1]),
        scale=float(scale),
        matrix_3x3=matrix_3x3,
        rms_error=float(rms_error),
        num_points=n_points
    )


def transform_points(points: np.ndarray, matrix_3x3: np.ndarray) -> np.ndarray:
    """Apply a 3x3 homogeneous transformation to a set of 2D points.
    
    Args:
        points: Nx2 array of points.
        matrix_3x3: 3x3 transformation matrix.
        
    Returns:
        Nx2 array of transformed points.
    """
    n = len(points)
    # Convert to homogeneous coordinates
    ones = np.ones((n, 1))
    points_h = np.hstack([points, ones])  # Nx3
    
    # Apply transformation
    transformed_h = (matrix_3x3 @ points_h.T).T  # Nx3
    
    # Convert back to Cartesian
    return transformed_h[:, :2]


def rigid_params_to_matrix(
    theta_deg: float,
    tx: float,
    ty: float,
    scale: float = 1.0
) -> np.ndarray:
    """Convert rigid parameters to 3x3 homogeneous transformation matrix.
    
    Args:
        theta_deg: Counter-clockwise rotation angle in degrees.
        tx: Translation in x direction.
        ty: Translation in y direction.
        scale: Uniform scale factor.
        
    Returns:
        3x3 transformation matrix.
    """
    theta_rad = math.radians(theta_deg)
    cos_t = math.cos(theta_rad)
    sin_t = math.sin(theta_rad)
    
    return np.array([
        [scale * cos_t, -scale * sin_t, tx],
        [scale * sin_t,  scale * cos_t, ty],
        [0.0, 0.0, 1.0]
    ])


def matrix_to_rigid_params(matrix_3x3: np.ndarray) -> Tuple[float, float, float, float]:
    """Extract rigid parameters from 3x3 homogeneous transformation matrix.
    
    Args:
        matrix_3x3: 3x3 transformation matrix.
        
    Returns:
        Tuple of (theta_deg, tx, ty, scale).
    """
    # Extract rotation and scale
    a = matrix_3x3[0, 0]
    b = matrix_3x3[0, 1]
    c = matrix_3x3[1, 0]
    d = matrix_3x3[1, 1]
    
    # Scale is the norm of the rotation columns
    scale = math.sqrt(a * a + c * c)
    
    # Rotation angle
    theta_rad = math.atan2(c / scale, a / scale)
    theta_deg = math.degrees(theta_rad)
    
    # Translation
    tx = matrix_3x3[0, 2]
    ty = matrix_3x3[1, 2]
    
    return (theta_deg, tx, ty, scale)


def compute_point_residuals(
    fixed_points: np.ndarray,
    moving_points: np.ndarray,
    matrix_3x3: np.ndarray
) -> Tuple[np.ndarray, float]:
    """Compute per-point residuals after applying transformation.
    
    Args:
        fixed_points: Nx2 array of fixed points.
        moving_points: Nx2 array of moving points.
        matrix_3x3: 3x3 transformation matrix.
        
    Returns:
        Tuple of (residuals Nx1, rms_error).
    """
    transformed = transform_points(moving_points, matrix_3x3)
    diff = fixed_points - transformed
    residuals = np.sqrt(np.sum(diff ** 2, axis=1))
    rms_error = np.sqrt(np.mean(residuals ** 2))
    return residuals, rms_error

"""
2D Affine Transformation Estimation.

Implements least-squares estimation for affine transformations between point sets.

The transformation maps points from the moving image to the fixed image:
    p_fixed = A * p_moving + t

Where A is a 2x2 matrix that can include rotation, scale (non-uniform), and shear.

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
class AffineTransformResult:
    """Result of affine transformation estimation."""
    theta_deg: float  # Rotation angle in degrees (counter-clockwise)
    tx: float         # Translation in x (column direction)
    ty: float         # Translation in y (row direction)
    scale_x: float    # Scale factor in x direction
    scale_y: float    # Scale factor in y direction
    shear: float      # Shear factor
    matrix_3x3: np.ndarray  # 3x3 homogeneous transformation matrix
    rms_error: float  # RMS residual error
    num_points: int   # Number of points used


# Keep old name for backward compatibility
RigidTransformResult = AffineTransformResult


class TransformEstimationError(Exception):
    """Exception raised when transformation estimation fails."""
    def __init__(self, message: str, error_code: str):
        super().__init__(message)
        self.error_code = error_code


def compute_affine_transform(
    fixed_points: np.ndarray,
    moving_points: np.ndarray
) -> AffineTransformResult:
    """Estimate 2D affine transformation using least-squares.
    
    Solves for the affine matrix A and translation t that minimizes:
        ||A * moving + t - fixed||^2
    
    The affine transformation includes: rotation, non-uniform scale, shear, and translation.
    Requires at least 3 non-collinear points for a unique solution.
    
    Args:
        fixed_points: Nx2 array of points in the fixed image (destination).
        moving_points: Nx2 array of corresponding points in the moving image (source).
        
    Returns:
        AffineTransformResult containing the estimated parameters.
        
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
    if n_points < 3:
        raise TransformEstimationError(
            f"Not enough points to estimate affine transform (got {n_points}, need at least 3)",
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
    
    # Build the linear system for affine transformation
    # For each point: [x', y'] = [a, b, tx; c, d, ty] @ [x, y, 1]^T
    # Rewrite as: [x'] = [x, y, 1, 0, 0, 0] @ [a, b, tx, c, d, ty]^T
    #             [y'] = [0, 0, 0, x, y, 1] @ [a, b, tx, c, d, ty]^T
    
    A = np.zeros((2 * n_points, 6))
    b = np.zeros(2 * n_points)
    
    for i in range(n_points):
        x, y = src_pts[i]
        x_dst, y_dst = dst_pts[i]
        
        # Equation for x'
        A[2*i, 0] = x
        A[2*i, 1] = y
        A[2*i, 2] = 1
        b[2*i] = x_dst
        
        # Equation for y'
        A[2*i+1, 3] = x
        A[2*i+1, 4] = y
        A[2*i+1, 5] = 1
        b[2*i+1] = y_dst
    
    # Solve using least squares
    try:
        params, residuals, rank, s = np.linalg.lstsq(A, b, rcond=None)
    except np.linalg.LinAlgError:
        raise TransformEstimationError(
            "Least squares failed: singular matrix",
            "SINGULAR_TRANSFORM"
        )
    
    # Check if solution is valid
    if rank < 6:
        # Could still work with less than full rank if points are sufficient
        pass
    
    # Extract parameters
    a, b_param, tx, c, d, ty = params
    
    # Build 3x3 transformation matrix
    matrix_3x3 = np.array([
        [a, b_param, tx],
        [c, d, ty],
        [0.0, 0.0, 1.0]
    ])
    
    # Decompose affine matrix into rotation, scale, and shear
    # A = R @ S @ Shear, where R is rotation, S is scale
    # Using polar decomposition / SVD decomposition
    
    # Extract the 2x2 linear part
    linear = matrix_3x3[0:2, 0:2]
    
    # SVD decomposition: linear = U @ Sigma @ V^T
    U, sigma, Vt = np.linalg.svd(linear)
    
    # Scale factors are the singular values
    scale_x = sigma[0]
    scale_y = sigma[1]
    
    # Check for reflection and correct
    if np.linalg.det(U) * np.linalg.det(Vt) < 0:
        # Flip sign to ensure proper rotation
        scale_y = -scale_y
        U[:, 1] *= -1
    
    # Rotation matrix R = U @ V^T
    R = U @ Vt
    
    # Rotation angle
    theta_rad = np.arctan2(R[1, 0], R[0, 0])
    theta_deg = np.degrees(theta_rad)
    
    # Compute shear (from the decomposition A = R @ [[sx, shear*sy], [0, sy]])
    # For simplicity, we compute shear as deviation from pure rotation+scale
    # shear = (a*c + b*d) / (scale_x * scale_y) approximately
    # A simpler approach: shear represents the off-diagonal contribution
    R_inv = R.T
    scaled_shear_matrix = R_inv @ linear
    # scaled_shear_matrix should be approximately [[sx, shear], [0, sy]]
    shear = scaled_shear_matrix[0, 1] / scale_y if abs(scale_y) > 1e-10 else 0.0
    
    # Compute RMS error
    transformed_src = transform_points(src_pts, matrix_3x3)
    residuals_pts = dst_pts - transformed_src
    rms_error = np.sqrt(np.mean(np.sum(residuals_pts ** 2, axis=1)))
    
    return AffineTransformResult(
        theta_deg=float(theta_deg),
        tx=float(tx),
        ty=float(ty),
        scale_x=float(scale_x),
        scale_y=float(scale_y),
        shear=float(shear),
        matrix_3x3=matrix_3x3,
        rms_error=float(rms_error),
        num_points=n_points
    )


def compute_similarity_transform(
    fixed_points: np.ndarray,
    moving_points: np.ndarray,
    allow_scale: bool = True
) -> AffineTransformResult:
    """Estimate 2D rigid or similarity transformation using Kabsch algorithm.
    
    Uses centroid alignment + SVD to find the optimal rotation and translation
    that maps moving_points to fixed_points.
    
    The transformation satisfies: p_fixed = s * R * p_moving + t
    
    Args:
        fixed_points: Nx2 array of points in the fixed image (destination).
        moving_points: Nx2 array of corresponding points in the moving image (source).
        allow_scale: If True, estimate uniform scale (similarity). If False, pure rigid.
        
    Returns:
        AffineTransformResult containing the estimated parameters.
        
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
        Vt[1, :] *= -1
        R = Vt.T @ U.T
    
    # Step 6: Compute scale (if requested)
    if allow_scale:
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
    t = mu_dst - scale * (R @ mu_src)
    
    # Step 8: Extract rotation angle
    theta_rad = np.arctan2(R[1, 0], R[0, 0])
    theta_deg = np.degrees(theta_rad)
    
    # Step 9: Build 3x3 homogeneous transformation matrix
    matrix_3x3 = np.eye(3)
    matrix_3x3[0:2, 0:2] = scale * R
    matrix_3x3[0:2, 2] = t
    
    # Step 10: Compute RMS error
    transformed_src = transform_points(src_pts, matrix_3x3)
    residuals = dst_pts - transformed_src
    rms_error = np.sqrt(np.mean(np.sum(residuals ** 2, axis=1)))
    
    return AffineTransformResult(
        theta_deg=float(theta_deg),
        tx=float(t[0]),
        ty=float(t[1]),
        scale_x=float(scale),
        scale_y=float(scale),
        shear=0.0,
        matrix_3x3=matrix_3x3,
        rms_error=float(rms_error),
        num_points=n_points
    )


def compute_transform(
    fixed_points: np.ndarray,
    moving_points: np.ndarray,
    mode: str = "affine"
) -> AffineTransformResult:
    """Estimate 2D transformation using specified mode.
    
    Args:
        fixed_points: Nx2 array of points in the fixed image (destination).
        moving_points: Nx2 array of corresponding points in the moving image (source).
        mode: Transform mode - "rigid", "similarity", or "affine".
        
    Returns:
        AffineTransformResult containing the estimated parameters.
    """
    mode = mode.lower()
    if mode == "rigid":
        return compute_similarity_transform(fixed_points, moving_points, allow_scale=False)
    elif mode == "similarity":
        return compute_similarity_transform(fixed_points, moving_points, allow_scale=True)
    elif mode == "affine":
        return compute_affine_transform(fixed_points, moving_points)
    else:
        raise TransformEstimationError(
            f"Unknown transform mode: {mode}. Use 'rigid', 'similarity', or 'affine'.",
            "INVALID_INPUT"
        )


def compute_rigid_transform(
    fixed_points: np.ndarray,
    moving_points: np.ndarray,
    allow_scale: bool = False
) -> AffineTransformResult:
    """Estimate 2D rigid (or similarity) transformation using Kabsch algorithm.
    
    DEPRECATED: Use compute_transform with mode parameter instead.
    
    Args:
        fixed_points: Nx2 array of points in the fixed image (destination).
        moving_points: Nx2 array of corresponding points in the moving image (source).
        allow_scale: If True, compute similarity transform. If False, compute rigid.
        
    Returns:
        AffineTransformResult containing the estimated parameters.
    """
    return compute_similarity_transform(fixed_points, moving_points, allow_scale)


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
    scale: float = 1.0,
    scale_x: Optional[float] = None,
    scale_y: Optional[float] = None
) -> np.ndarray:
    """Convert rigid/affine parameters to 3x3 homogeneous transformation matrix.
    
    Args:
        theta_deg: Counter-clockwise rotation angle in degrees.
        tx: Translation in x direction.
        ty: Translation in y direction.
        scale: Uniform scale factor (used if scale_x/scale_y not provided).
        scale_x: Scale factor in x direction (overrides scale).
        scale_y: Scale factor in y direction (overrides scale).
        
    Returns:
        3x3 transformation matrix.
    """
    theta_rad = math.radians(theta_deg)
    cos_t = math.cos(theta_rad)
    sin_t = math.sin(theta_rad)
    
    # Use individual scales if provided, otherwise use uniform scale
    sx = scale_x if scale_x is not None else scale
    sy = scale_y if scale_y is not None else scale
    
    return np.array([
        [sx * cos_t, -sy * sin_t, tx],
        [sx * sin_t,  sy * cos_t, ty],
        [0.0, 0.0, 1.0]
    ])


def affine_params_to_matrix(
    theta_deg: float,
    tx: float,
    ty: float,
    scale_x: float = 1.0,
    scale_y: float = 1.0,
    shear: float = 0.0
) -> np.ndarray:
    """Convert affine parameters to 3x3 homogeneous transformation matrix.
    
    The transformation is: T = Translation @ Rotation @ Scale @ Shear
    
    Args:
        theta_deg: Counter-clockwise rotation angle in degrees.
        tx: Translation in x direction.
        ty: Translation in y direction.
        scale_x: Scale factor in x direction.
        scale_y: Scale factor in y direction.
        shear: Shear factor.
        
    Returns:
        3x3 transformation matrix.
    """
    theta_rad = math.radians(theta_deg)
    cos_t = math.cos(theta_rad)
    sin_t = math.sin(theta_rad)
    
    # Build rotation matrix
    R = np.array([
        [cos_t, -sin_t],
        [sin_t, cos_t]
    ])
    
    # Build scale + shear matrix
    S = np.array([
        [scale_x, shear * scale_y],
        [0, scale_y]
    ])
    
    # Combined linear part
    linear = R @ S
    
    return np.array([
        [linear[0, 0], linear[0, 1], tx],
        [linear[1, 0], linear[1, 1], ty],
        [0.0, 0.0, 1.0]
    ])


def matrix_to_rigid_params(matrix_3x3: np.ndarray) -> Tuple[float, float, float, float]:
    """Extract rigid parameters from 3x3 homogeneous transformation matrix.
    
    Note: This extracts uniform scale only. For full affine decomposition,
    use matrix_to_affine_params.
    
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
    theta_rad = math.atan2(c / scale, a / scale) if scale > 1e-10 else 0.0
    theta_deg = math.degrees(theta_rad)
    
    # Translation
    tx = matrix_3x3[0, 2]
    ty = matrix_3x3[1, 2]
    
    return (theta_deg, tx, ty, scale)


def matrix_to_affine_params(matrix_3x3: np.ndarray) -> Tuple[float, float, float, float, float, float]:
    """Extract affine parameters from 3x3 homogeneous transformation matrix.
    
    Uses SVD decomposition to extract rotation, non-uniform scale, and shear.
    
    Args:
        matrix_3x3: 3x3 transformation matrix.
        
    Returns:
        Tuple of (theta_deg, tx, ty, scale_x, scale_y, shear).
    """
    # Extract the 2x2 linear part
    linear = matrix_3x3[0:2, 0:2]
    
    # SVD decomposition: linear = U @ Sigma @ V^T
    U, sigma, Vt = np.linalg.svd(linear)
    
    # Scale factors are the singular values
    scale_x = sigma[0]
    scale_y = sigma[1]
    
    # Check for reflection and correct
    if np.linalg.det(U) * np.linalg.det(Vt) < 0:
        scale_y = -scale_y
        U[:, 1] *= -1
    
    # Rotation matrix R = U @ V^T
    R = U @ Vt
    
    # Rotation angle
    theta_rad = np.arctan2(R[1, 0], R[0, 0])
    theta_deg = np.degrees(theta_rad)
    
    # Compute shear
    R_inv = R.T
    scaled_shear_matrix = R_inv @ linear
    shear = scaled_shear_matrix[0, 1] / scale_y if abs(scale_y) > 1e-10 else 0.0
    
    # Translation
    tx = matrix_3x3[0, 2]
    ty = matrix_3x3[1, 2]
    
    return (theta_deg, tx, ty, scale_x, scale_y, shear)


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

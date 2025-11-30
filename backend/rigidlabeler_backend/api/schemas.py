"""
Pydantic schemas for RigidLabeler API.

Defines data models for request/response objects following the API specification.
"""

from typing import List, Optional, Any
from pydantic import BaseModel, Field
from datetime import datetime


# ============================================================================
# Basic Data Models
# ============================================================================

class Point2D(BaseModel):
    """2D point in pixel coordinates.
    
    Coordinate system:
    - Origin: top-left corner of image
    - x: column coordinate (increases rightward)
    - y: row coordinate (increases downward)
    """
    x: float = Field(..., description="Column coordinate (pixels)")
    y: float = Field(..., description="Row coordinate (pixels)")


class TiePoint(BaseModel):
    """A pair of corresponding points between fixed and moving images."""
    fixed: Point2D = Field(..., description="Point on fixed/base image")
    moving: Point2D = Field(..., description="Point on moving/warp image")


class RigidParams(BaseModel):
    """2D rigid transformation parameters (with optional uniform scale).
    
    The transformation maps points from moving image to fixed image:
    p_fixed = scale * R(theta) * p_moving + [tx, ty]
    """
    theta_deg: float = Field(..., description="Counter-clockwise rotation angle in degrees")
    tx: float = Field(..., description="Translation in x (column) direction, pixels")
    ty: float = Field(..., description="Translation in y (row) direction, pixels")
    scale: float = Field(default=1.0, description="Uniform scale factor (1.0 for pure rigid)")


class LabelMeta(BaseModel):
    """Optional metadata for a label."""
    comment: Optional[str] = Field(default=None, description="User comment or notes")
    timestamp: Optional[str] = Field(default=None, description="ISO format timestamp")
    
    @classmethod
    def create_now(cls, comment: Optional[str] = None) -> "LabelMeta":
        """Create metadata with current timestamp."""
        return cls(
            comment=comment,
            timestamp=datetime.now().isoformat()
        )


class Label(BaseModel):
    """Complete label object for a pair of images.
    
    Contains all information needed to describe the rigid transformation
    between a fixed and moving image pair.
    """
    image_fixed: str = Field(..., description="Path to fixed/base image")
    image_moving: str = Field(..., description="Path to moving/warp image")
    rigid: RigidParams = Field(..., description="Rigid transformation parameters")
    matrix_3x3: List[List[float]] = Field(
        ..., 
        description="3x3 homogeneous transformation matrix (row-major)"
    )
    tie_points: List[TiePoint] = Field(
        default_factory=list,
        description="List of tie points used for estimation"
    )
    meta: Optional[LabelMeta] = Field(default=None, description="Optional metadata")


# ============================================================================
# API Response Models
# ============================================================================

class ApiResponse(BaseModel):
    """Base response model for all API endpoints."""
    status: str = Field(..., description="'ok' or 'error'")
    message: Optional[str] = Field(default=None, description="Human-readable message")
    error_code: Optional[str] = Field(default=None, description="Error code if status='error'")
    data: Optional[Any] = Field(default=None, description="Response payload")
    
    @classmethod
    def ok(cls, data: Any = None, message: Optional[str] = None) -> "ApiResponse":
        """Create a success response."""
        return cls(status="ok", data=data, message=message)
    
    @classmethod
    def error(cls, error_code: str, message: str) -> "ApiResponse":
        """Create an error response."""
        return cls(status="error", error_code=error_code, message=message, data=None)


class HealthInfo(BaseModel):
    """Health check response data."""
    version: str = Field(..., description="Backend version")
    backend: str = Field(default="fastapi", description="Backend framework name")


class ComputeRigidResult(BaseModel):
    """Result of rigid transformation computation."""
    rigid: RigidParams = Field(..., description="Estimated rigid parameters")
    matrix_3x3: List[List[float]] = Field(..., description="3x3 transformation matrix")
    rms_error: float = Field(..., description="RMS residual error in pixels")
    num_points: int = Field(..., description="Number of points used for estimation")


class LabelSaveResult(BaseModel):
    """Result of saving a label."""
    label_path: str = Field(..., description="Path where label was saved")
    label_id: str = Field(..., description="Unique identifier for the label")


class LabelListItem(BaseModel):
    """Summary info for a label in the list."""
    label_id: str = Field(..., description="Label identifier")
    label_path: str = Field(..., description="Path to label file")
    image_fixed: str = Field(..., description="Fixed image path")
    image_moving: str = Field(..., description="Moving image path")


class WarpPreviewResult(BaseModel):
    """Result of generating a warp preview."""
    preview_path: str = Field(..., description="Path to generated preview image")


# ============================================================================
# API Request Models
# ============================================================================

class ComputeRigidRequest(BaseModel):
    """Request body for POST /compute/rigid."""
    image_fixed: Optional[str] = Field(
        default=None, 
        description="Fixed image path (optional, for logging)"
    )
    image_moving: Optional[str] = Field(
        default=None,
        description="Moving image path (optional, for logging)"
    )
    tie_points: List[TiePoint] = Field(..., description="List of tie points")
    allow_scale: bool = Field(
        default=False,
        description="If true, estimate similarity transform (R + scale + t)"
    )
    min_points_required: int = Field(
        default=2,
        description="Minimum number of points required"
    )


class LabelSaveRequest(BaseModel):
    """Request body for POST /labels/save."""
    image_fixed: str = Field(..., description="Fixed image path")
    image_moving: str = Field(..., description="Moving image path")
    rigid: RigidParams = Field(..., description="Rigid transformation parameters")
    matrix_3x3: List[List[float]] = Field(..., description="3x3 transformation matrix")
    tie_points: List[TiePoint] = Field(
        default_factory=list,
        description="List of tie points"
    )
    meta: Optional[LabelMeta] = Field(default=None, description="Optional metadata")


class WarpPreviewRequest(BaseModel):
    """Request body for POST /warp/preview."""
    image_fixed: str = Field(..., description="Fixed image path (for output size)")
    image_moving: str = Field(..., description="Moving image path")
    rigid: Optional[RigidParams] = Field(
        default=None,
        description="Rigid parameters (alternative to matrix_3x3)"
    )
    matrix_3x3: Optional[List[List[float]]] = Field(
        default=None,
        description="3x3 transformation matrix (alternative to rigid)"
    )
    output_name: Optional[str] = Field(
        default=None,
        description="Output filename (auto-generated if not provided)"
    )


class CheckerboardPreviewRequest(BaseModel):
    """Request body for POST /warp/checkerboard."""
    image_fixed: str = Field(..., description="Fixed image path")
    image_moving: str = Field(..., description="Moving image path")
    rigid: Optional[RigidParams] = Field(
        default=None,
        description="Rigid parameters (alternative to matrix_3x3)"
    )
    matrix_3x3: Optional[List[List[float]]] = Field(
        default=None,
        description="3x3 transformation matrix (alternative to rigid)"
    )
    board_size: int = Field(
        default=8,
        ge=2,
        le=64,
        description="Number of grid cells per row/column (2-64)"
    )
    use_center_origin: bool = Field(
        default=False,
        description="If true, the matrix was computed with image center as origin"
    )


class CheckerboardPreviewResult(BaseModel):
    """Result of generating a checkerboard preview."""
    image_base64: str = Field(..., description="Base64-encoded PNG image")
    width: int = Field(..., description="Image width in pixels")
    height: int = Field(..., description="Image height in pixels")


# ============================================================================
# Error Codes
# ============================================================================

class ErrorCode:
    """Standard error codes for the API."""
    INVALID_INPUT = "INVALID_INPUT"
    NOT_ENOUGH_POINTS = "NOT_ENOUGH_POINTS"
    SINGULAR_TRANSFORM = "SINGULAR_TRANSFORM"
    LABEL_NOT_FOUND = "LABEL_NOT_FOUND"
    IO_ERROR = "IO_ERROR"
    INTERNAL_ERROR = "INTERNAL_ERROR"

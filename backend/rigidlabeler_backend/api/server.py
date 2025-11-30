"""
FastAPI server for RigidLabeler backend.

Provides REST API endpoints for:
- Health check
- Rigid transformation computation
- Label save/load operations
- (Optional) Warp preview generation
"""

import numpy as np
from fastapi import FastAPI, Query, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from typing import Optional, List
from contextlib import asynccontextmanager
import logging

from .schemas import (
    ApiResponse, HealthInfo, ErrorCode,
    ComputeRigidRequest, ComputeRigidResult,
    LabelSaveRequest, LabelSaveResult,
    Label, LabelListItem, TiePoint, RigidParams,
    WarpPreviewRequest, WarpPreviewResult
)
from ..config import get_config
from ..core.transforms import (
    compute_rigid_transform, 
    TransformEstimationError,
    rigid_params_to_matrix
)
from ..io.label_store import (
    save_label, load_label, list_labels,
    LabelStoreError
)
from ..io.image_loader import (
    load_image, get_image_size, save_image, warp_image,
    ImageLoadError, HAS_OPENCV
)
from ..utils.logging_utils import setup_logging, get_logger
from .. import __version__


# Setup logging
logger = get_logger()


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan handler."""
    # Startup
    config = get_config()
    setup_logging(level=config.logging.level)
    logger.info(f"RigidLabeler backend v{__version__} starting...")
    logger.info(f"Labels root: {config.paths.labels_root}")
    logger.info(f"Temp root: {config.paths.temp_root}")
    yield
    # Shutdown
    logger.info("RigidLabeler backend shutting down...")


# Create FastAPI app
app = FastAPI(
    title="RigidLabeler Backend",
    description="Backend API for 2D rigid transformation labeling tool",
    version=__version__,
    lifespan=lifespan
)

# Add CORS middleware for local development
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


# ============================================================================
# Health Check
# ============================================================================

@app.get("/health", response_model=ApiResponse)
async def health_check():
    """Health check endpoint.
    
    Returns basic backend info to confirm the server is running.
    """
    return ApiResponse.ok(
        data=HealthInfo(version=__version__, backend="fastapi"),
        message="rigidlabeler backend alive"
    )


# ============================================================================
# Rigid Transform Computation
# ============================================================================

@app.post("/compute/rigid", response_model=ApiResponse)
async def compute_rigid(request: ComputeRigidRequest):
    """Compute 2D rigid transformation from tie points.
    
    Uses Procrustes analysis (centroid alignment + SVD) to estimate
    the optimal rigid transformation mapping moving points to fixed points.
    """
    # Validate minimum points
    if len(request.tie_points) < request.min_points_required:
        return ApiResponse.error(
            ErrorCode.NOT_ENOUGH_POINTS,
            f"Not enough points to estimate rigid transform "
            f"(got {len(request.tie_points)}, need at least {request.min_points_required})"
        )
    
    # Extract point coordinates
    fixed_points = np.array([
        [tp.fixed.x, tp.fixed.y] for tp in request.tie_points
    ])
    moving_points = np.array([
        [tp.moving.x, tp.moving.y] for tp in request.tie_points
    ])
    
    try:
        result = compute_rigid_transform(
            fixed_points=fixed_points,
            moving_points=moving_points,
            allow_scale=request.allow_scale
        )
        
        return ApiResponse.ok(
            data=ComputeRigidResult(
                rigid=RigidParams(
                    theta_deg=result.theta_deg,
                    tx=result.tx,
                    ty=result.ty,
                    scale=result.scale
                ),
                matrix_3x3=result.matrix_3x3.tolist(),
                rms_error=result.rms_error,
                num_points=result.num_points
            )
        )
        
    except TransformEstimationError as e:
        return ApiResponse.error(e.error_code, str(e))
    
    except Exception as e:
        logger.exception("Unexpected error in compute_rigid")
        return ApiResponse.error(
            ErrorCode.INTERNAL_ERROR,
            f"Internal error: {str(e)}"
        )


# ============================================================================
# Label Operations
# ============================================================================

@app.post("/labels/save", response_model=ApiResponse)
async def save_label_endpoint(request: LabelSaveRequest):
    """Save a label for an image pair.
    
    Stores the rigid transformation parameters, matrix, and tie points
    in a JSON file.
    """
    try:
        # Create Label object from request
        label = Label(
            image_fixed=request.image_fixed,
            image_moving=request.image_moving,
            rigid=request.rigid,
            matrix_3x3=request.matrix_3x3,
            tie_points=request.tie_points,
            meta=request.meta
        )
        
        result = save_label(label)
        
        return ApiResponse.ok(
            data=result,
            message="Label saved"
        )
        
    except LabelStoreError as e:
        return ApiResponse.error(e.error_code, str(e))
    
    except Exception as e:
        logger.exception("Unexpected error in save_label")
        return ApiResponse.error(
            ErrorCode.INTERNAL_ERROR,
            f"Internal error: {str(e)}"
        )


@app.get("/labels/load", response_model=ApiResponse)
async def load_label_endpoint(
    image_fixed: str = Query(..., description="Path to fixed image"),
    image_moving: str = Query(..., description="Path to moving image")
):
    """Load a label for an image pair.
    
    Retrieves the stored label including rigid parameters, matrix,
    and tie points.
    """
    try:
        label = load_label(image_fixed, image_moving)
        
        return ApiResponse.ok(data=label)
        
    except LabelStoreError as e:
        return ApiResponse.error(e.error_code, str(e))
    
    except Exception as e:
        logger.exception("Unexpected error in load_label")
        return ApiResponse.error(
            ErrorCode.INTERNAL_ERROR,
            f"Internal error: {str(e)}"
        )


@app.get("/labels/list", response_model=ApiResponse)
async def list_labels_endpoint(
    project: Optional[str] = Query(None, description="Project name (optional)")
):
    """List all available labels.
    
    Returns a list of label summaries including image paths and IDs.
    """
    try:
        labels = list_labels()
        
        return ApiResponse.ok(data=labels)
        
    except Exception as e:
        logger.exception("Unexpected error in list_labels")
        return ApiResponse.error(
            ErrorCode.INTERNAL_ERROR,
            f"Internal error: {str(e)}"
        )


# ============================================================================
# Warp Preview (Optional)
# ============================================================================

@app.post("/warp/preview", response_model=ApiResponse)
async def warp_preview_endpoint(request: WarpPreviewRequest):
    """Generate a warp preview image.
    
    Applies the transformation to the moving image and saves
    the result as a preview file.
    """
    if not HAS_OPENCV:
        return ApiResponse.error(
            ErrorCode.INTERNAL_ERROR,
            "OpenCV is required for warp preview. Install opencv-python."
        )
    
    try:
        config = get_config()
        
        # Get transformation matrix
        if request.matrix_3x3 is not None:
            matrix = np.array(request.matrix_3x3)
        elif request.rigid is not None:
            matrix = rigid_params_to_matrix(
                request.rigid.theta_deg,
                request.rigid.tx,
                request.rigid.ty,
                request.rigid.scale
            )
        else:
            return ApiResponse.error(
                ErrorCode.INVALID_INPUT,
                "Must provide either 'rigid' or 'matrix_3x3'"
            )
        
        # Load images
        try:
            fixed_size = get_image_size(request.image_fixed)
            moving_img = load_image(request.image_moving)
        except ImageLoadError as e:
            return ApiResponse.error(e.error_code, str(e))
        
        # Warp the moving image
        warped = warp_image(moving_img, matrix, fixed_size)
        
        # Generate output filename
        if request.output_name:
            output_name = request.output_name
        else:
            from pathlib import Path
            fixed_stem = Path(request.image_fixed).stem
            moving_stem = Path(request.image_moving).stem
            output_name = f"preview_{fixed_stem}_{moving_stem}.png"
        
        # Save preview
        import os
        preview_path = os.path.join(config.paths.temp_root, output_name)
        save_image(preview_path, warped)
        
        return ApiResponse.ok(
            data=WarpPreviewResult(preview_path=preview_path)
        )
        
    except Exception as e:
        logger.exception("Unexpected error in warp_preview")
        return ApiResponse.error(
            ErrorCode.INTERNAL_ERROR,
            f"Internal error: {str(e)}"
        )


# ============================================================================
# Error Handlers
# ============================================================================

@app.exception_handler(Exception)
async def global_exception_handler(request, exc):
    """Global exception handler for uncaught exceptions."""
    logger.exception(f"Unhandled exception: {exc}")
    return ApiResponse.error(
        ErrorCode.INTERNAL_ERROR,
        f"Internal server error: {str(exc)}"
    )

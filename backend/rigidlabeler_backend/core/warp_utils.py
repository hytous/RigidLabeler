"""
Image warping utilities using PyTorch.

Uses PyTorch's grid_sample for image transformation, which:
1. Supports Chinese file paths (unlike OpenCV)
2. Uses image center as origin by default (normalized coordinates from -1 to 1)
3. Provides differentiable operations
"""

import numpy as np
from pathlib import Path
from typing import Tuple, Optional
from PIL import Image
import io
import base64

try:
    import torch
    import torch.nn.functional as F
    HAS_TORCH = True
except ImportError:
    HAS_TORCH = False


def load_image_pil(path: str) -> np.ndarray:
    """Load an image using PIL (supports Chinese paths).
    
    Args:
        path: Path to the image file.
        
    Returns:
        Image as numpy array (H, W, C) in RGB format.
        
    Raises:
        IOError: If loading fails.
    """
    try:
        pil_img = Image.open(path)
        # Convert to RGB if necessary
        if pil_img.mode == 'L':
            pil_img = pil_img.convert('RGB')
        elif pil_img.mode == 'RGBA':
            pil_img = pil_img.convert('RGB')
        elif pil_img.mode != 'RGB':
            pil_img = pil_img.convert('RGB')
        return np.array(pil_img)
    except Exception as e:
        raise IOError(f"Failed to load image: {path}, error: {e}")


def numpy_to_tensor(image: np.ndarray) -> "torch.Tensor":
    """Convert numpy image (H, W, C) to tensor (1, C, H, W).
    
    Args:
        image: Image as numpy array (H, W, C) with values 0-255.
        
    Returns:
        Image as tensor (1, C, H, W) with values 0-1.
    """
    if not HAS_TORCH:
        raise RuntimeError("PyTorch is required for this operation")
    
    # Normalize to [0, 1]
    tensor = torch.from_numpy(image.astype(np.float32) / 255.0)
    
    # (H, W, C) -> (C, H, W)
    tensor = tensor.permute(2, 0, 1)
    
    # Add batch dimension: (C, H, W) -> (1, C, H, W)
    tensor = tensor.unsqueeze(0)
    
    return tensor


def tensor_to_numpy(tensor: "torch.Tensor") -> np.ndarray:
    """Convert tensor (1, C, H, W) or (C, H, W) to numpy image (H, W, C).
    
    Args:
        tensor: Image tensor with values 0-1.
        
    Returns:
        Image as numpy array (H, W, C) with values 0-255.
    """
    if tensor.dim() == 4:
        tensor = tensor.squeeze(0)  # (1, C, H, W) -> (C, H, W)
    
    # (C, H, W) -> (H, W, C)
    tensor = tensor.permute(1, 2, 0)
    
    # Convert to numpy and scale to 0-255
    array = tensor.cpu().numpy()
    array = np.clip(array * 255, 0, 255).astype(np.uint8)
    
    return array


def create_affine_grid(
    matrix_3x3: np.ndarray,
    output_size: Tuple[int, int],
    input_size: Tuple[int, int],
    use_center_origin: bool = False
) -> "torch.Tensor":
    """Create sampling grid for affine transformation.
    
    PyTorch's grid_sample uses normalized coordinates [-1, 1] where:
    - (-1, -1) is the top-left corner
    - (1, 1) is the bottom-right corner
    - (0, 0) is the image center
    
    The matrix_3x3 maps moving image coordinates to fixed image coordinates:
        p_fixed = M @ p_moving
    
    For warping, we need the inverse: given a fixed image pixel, find the
    corresponding moving image pixel to sample from.
    
    Args:
        matrix_3x3: 3x3 transformation matrix (moving -> fixed).
        output_size: (height, width) of output (fixed image size).
        input_size: (height, width) of input (moving image size).
        use_center_origin: If True, the matrix was computed with center origin.
        
    Returns:
        Sampling grid tensor of shape (1, H, W, 2).
    """
    if not HAS_TORCH:
        raise RuntimeError("PyTorch is required for this operation")
    
    H_out, W_out = output_size  # Fixed image size
    H_in, W_in = input_size     # Moving image size
    
    # Create base grid in normalized coordinates [-1, 1] for the output
    y = torch.linspace(-1, 1, H_out)
    x = torch.linspace(-1, 1, W_out)
    grid_y, grid_x = torch.meshgrid(y, x, indexing='ij')
    
    # Stack to (H, W, 2)
    grid = torch.stack([grid_x, grid_y], dim=-1)
    
    # Reshape to (H*W, 2) for transformation
    grid_flat = grid.reshape(-1, 2)
    
    # Convert matrix to torch
    M = torch.from_numpy(matrix_3x3.astype(np.float32))
    
    if use_center_origin:
        # Matrix was computed with center origin for both images
        # Fixed image center: (0, 0) in center-origin coords
        # Moving image center: (0, 0) in center-origin coords
        
        # Convert output grid from normalized [-1,1] to center-origin pixels
        # For fixed image: x_pixel = x_norm * (W_out/2), y_pixel = y_norm * (H_out/2)
        grid_pixel = grid_flat.clone()
        grid_pixel[:, 0] = grid_flat[:, 0] * (W_out / 2.0)
        grid_pixel[:, 1] = grid_flat[:, 1] * (H_out / 2.0)
        
        # Add homogeneous coordinate
        ones = torch.ones(grid_pixel.shape[0], 1)
        grid_homo = torch.cat([grid_pixel, ones], dim=1)  # (H*W, 3)
        
        # Apply inverse transformation to get source (moving) coordinates
        # dest = M @ source => source = M^(-1) @ dest
        M_inv = torch.inverse(M)
        source_homo = (M_inv @ grid_homo.T).T  # (H*W, 3)
        source_pixel = source_homo[:, :2] / source_homo[:, 2:3]
        
        # Convert to normalized coordinates for the MOVING image (input)
        # For moving image: x_norm = x_pixel / (W_in/2), y_norm = y_pixel / (H_in/2)
        source_norm = source_pixel.clone()
        source_norm[:, 0] = source_pixel[:, 0] / (W_in / 2.0)
        source_norm[:, 1] = source_pixel[:, 1] / (H_in / 2.0)
        
    else:
        # Matrix was computed with top-left origin
        # Convert output grid from normalized to pixel coordinates (top-left origin)
        grid_pixel = grid_flat.clone()
        grid_pixel[:, 0] = (grid_flat[:, 0] + 1) / 2.0 * (W_out - 1)
        grid_pixel[:, 1] = (grid_flat[:, 1] + 1) / 2.0 * (H_out - 1)
        
        # Add homogeneous coordinate
        ones = torch.ones(grid_pixel.shape[0], 1)
        grid_homo = torch.cat([grid_pixel, ones], dim=1)  # (H*W, 3)
        
        # Apply inverse transformation to get source coordinates
        M_inv = torch.inverse(M)
        source_homo = (M_inv @ grid_homo.T).T  # (H*W, 3)
        source_pixel = source_homo[:, :2] / source_homo[:, 2:3]
        
        # Convert to normalized coordinates for the MOVING image (input)
        source_norm = source_pixel.clone()
        source_norm[:, 0] = source_pixel[:, 0] / (W_in - 1) * 2.0 - 1.0
        source_norm[:, 1] = source_pixel[:, 1] / (H_in - 1) * 2.0 - 1.0
    
    # Reshape back to (1, H, W, 2)
    sampling_grid = source_norm.reshape(1, H_out, W_out, 2)
    
    return sampling_grid


def warp_image_torch(
    image: np.ndarray,
    matrix_3x3: np.ndarray,
    output_size: Tuple[int, int],
    use_center_origin: bool = False
) -> np.ndarray:
    """Warp an image using PyTorch grid_sample.
    
    Args:
        image: Input image (H, W, C) as numpy array (moving image).
        matrix_3x3: 3x3 transformation matrix (moving -> fixed).
        output_size: (height, width) of output (fixed image size).
        use_center_origin: If True, the matrix was computed with center origin.
        
    Returns:
        Warped image (H, W, C) as numpy array, same size as output_size.
    """
    if not HAS_TORCH:
        raise RuntimeError("PyTorch is required for image warping")
    
    # Get input size from the image
    input_size = (image.shape[0], image.shape[1])
    
    # Convert to tensor
    img_tensor = numpy_to_tensor(image)
    
    # Create sampling grid with both input and output sizes
    grid = create_affine_grid(matrix_3x3, output_size, input_size, use_center_origin)
    
    # Apply grid sample
    warped_tensor = F.grid_sample(
        img_tensor,
        grid,
        mode='bilinear',
        padding_mode='zeros',
        align_corners=True
    )
    
    # Convert back to numpy
    warped = tensor_to_numpy(warped_tensor)
    
    return warped


# Alias for backward compatibility
warp_image_pytorch = warp_image_torch


def create_checkerboard(
    fixed_image: np.ndarray,
    warped_moving: np.ndarray,
    board_size: int = 8
) -> np.ndarray:
    """Create checkerboard visualization from two images.
    
    Args:
        fixed_image: Fixed/reference image (H, W, C).
        warped_moving: Warped moving image (H, W, C), same size as fixed.
        board_size: Number of grid cells per row/column.
        
    Returns:
        Checkerboard image (H, W, C).
    """
    H, W = fixed_image.shape[:2]
    
    # Ensure both images have same size
    if warped_moving.shape[:2] != (H, W):
        raise ValueError(
            f"Image size mismatch: fixed={fixed_image.shape[:2]}, "
            f"moving={warped_moving.shape[:2]}"
        )
    
    # Ensure 3 channels
    if fixed_image.ndim == 2:
        fixed_image = np.stack([fixed_image] * 3, axis=-1)
    if warped_moving.ndim == 2:
        warped_moving = np.stack([warped_moving] * 3, axis=-1)
    
    # Create checkerboard mask
    cell_h = H // board_size
    cell_w = W // board_size
    checkerboard_mask = np.zeros((H, W), dtype=bool)
    
    for i in range(board_size):
        for j in range(board_size):
            if (i + j) % 2 == 0:
                y_start = i * cell_h
                y_end = (i + 1) * cell_h if i < board_size - 1 else H
                x_start = j * cell_w
                x_end = (j + 1) * cell_w if j < board_size - 1 else W
                checkerboard_mask[y_start:y_end, x_start:x_end] = True
    
    # Composite image
    result = np.zeros((H, W, 3), dtype=np.uint8)
    result[checkerboard_mask] = fixed_image[checkerboard_mask]
    result[~checkerboard_mask] = warped_moving[~checkerboard_mask]
    
    return result


def generate_checkerboard_preview(
    fixed_path: str,
    moving_path: str,
    matrix_3x3: np.ndarray,
    board_size: int = 8,
    use_center_origin: bool = False
) -> Tuple[str, int, int]:
    """Generate a checkerboard preview image.
    
    Args:
        fixed_path: Path to fixed/reference image.
        moving_path: Path to moving image.
        matrix_3x3: 3x3 transformation matrix.
        board_size: Number of grid cells.
        use_center_origin: If True, matrix was computed with center origin.
        
    Returns:
        Tuple of (base64_encoded_png, width, height).
    """
    if not HAS_TORCH:
        raise RuntimeError("PyTorch is required for checkerboard preview")
    
    # Load images using PIL (supports Chinese paths)
    fixed_img = load_image_pil(fixed_path)
    moving_img = load_image_pil(moving_path)
    
    # Get output size from fixed image
    H, W = fixed_img.shape[:2]
    
    # Warp moving image to align with fixed
    warped_moving = warp_image_torch(
        moving_img,
        matrix_3x3,
        (H, W),
        use_center_origin
    )
    
    # Create checkerboard
    checkerboard = create_checkerboard(fixed_img, warped_moving, board_size)
    
    # Encode to base64 PNG
    pil_img = Image.fromarray(checkerboard)
    buffer = io.BytesIO()
    pil_img.save(buffer, format='PNG')
    buffer.seek(0)
    base64_data = base64.b64encode(buffer.getvalue()).decode('utf-8')
    
    return base64_data, W, H

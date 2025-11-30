"""
Image loading utilities.

Provides functions for loading and processing images using OpenCV/PIL.
"""

import os
from pathlib import Path
from typing import Optional, Tuple
import numpy as np

try:
    import cv2
    HAS_OPENCV = True
except ImportError:
    HAS_OPENCV = False

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False


class ImageLoadError(Exception):
    """Exception for image loading operations."""
    def __init__(self, message: str, error_code: str = "IO_ERROR"):
        super().__init__(message)
        self.error_code = error_code


def load_image(path: str) -> np.ndarray:
    """Load an image from disk using PIL.
    
    Args:
        path: Path to the image file.
        
    Returns:
        Image as numpy array (H, W, C) in RGB format (if color).
        
    Raises:
        ImageLoadError: If loading fails.
    """
    if not os.path.exists(path):
        raise ImageLoadError(f"Image file not found: {path}")
    
    if not HAS_PIL:
        raise ImageLoadError(
            "PIL is required for image loading. Install Pillow."
        )
    
    try:
        pil_img = Image.open(path)
        # Convert to RGB if necessary
        if pil_img.mode == 'RGBA':
            pil_img = pil_img.convert('RGB')
        elif pil_img.mode not in ('RGB', 'L'):
            pil_img = pil_img.convert('RGB')
        img = np.array(pil_img)
        return img
    except Exception as e:
        raise ImageLoadError(f"Failed to load image with PIL: {e}")


def get_image_size(path: str) -> Tuple[int, int]:
    """Get image dimensions without loading full image.
    
    Args:
        path: Path to the image file.
        
    Returns:
        Tuple of (height, width).
        
    Raises:
        ImageLoadError: If reading fails.
    """
    if not os.path.exists(path):
        raise ImageLoadError(f"Image file not found: {path}")
    
    if not HAS_PIL:
        raise ImageLoadError(
            "PIL is required for image operations. Install Pillow."
        )
    
    try:
        with Image.open(path) as img:
            width, height = img.size
            return (height, width)
    except Exception as e:
        raise ImageLoadError(f"Failed to get image size: {e}")


def save_image(path: str, image: np.ndarray) -> None:
    """Save an image to disk using PIL.
    
    Args:
        path: Output path.
        image: Image array to save (RGB format).
        
    Raises:
        ImageLoadError: If saving fails.
    """
    if not HAS_PIL:
        raise ImageLoadError(
            "PIL is required for image operations. Install Pillow."
        )
    
    # Ensure directory exists
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    
    try:
        pil_img = Image.fromarray(image)
        pil_img.save(path)
    except Exception as e:
        raise ImageLoadError(f"Failed to save image: {e}")

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
    """Load an image from disk.
    
    Tries OpenCV first, falls back to PIL.
    
    Args:
        path: Path to the image file.
        
    Returns:
        Image as numpy array (H, W, C) in BGR format (if color).
        
    Raises:
        ImageLoadError: If loading fails.
    """
    if not os.path.exists(path):
        raise ImageLoadError(f"Image file not found: {path}")
    
    if HAS_OPENCV:
        img = cv2.imread(path, cv2.IMREAD_UNCHANGED)
        if img is None:
            raise ImageLoadError(f"Failed to load image with OpenCV: {path}")
        return img
    
    elif HAS_PIL:
        try:
            pil_img = Image.open(path)
            img = np.array(pil_img)
            # Convert RGB to BGR if color image
            if len(img.shape) == 3 and img.shape[2] == 3:
                img = img[:, :, ::-1].copy()
            return img
        except Exception as e:
            raise ImageLoadError(f"Failed to load image with PIL: {e}")
    
    else:
        raise ImageLoadError(
            "No image loading library available. Install opencv-python or Pillow."
        )


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
    
    if HAS_PIL:
        try:
            with Image.open(path) as img:
                width, height = img.size
                return (height, width)
        except Exception as e:
            raise ImageLoadError(f"Failed to get image size: {e}")
    
    elif HAS_OPENCV:
        img = cv2.imread(path, cv2.IMREAD_UNCHANGED)
        if img is None:
            raise ImageLoadError(f"Failed to read image: {path}")
        return img.shape[:2]
    
    else:
        raise ImageLoadError(
            "No image loading library available. Install opencv-python or Pillow."
        )


def save_image(path: str, image: np.ndarray) -> None:
    """Save an image to disk.
    
    Args:
        path: Output path.
        image: Image array to save.
        
    Raises:
        ImageLoadError: If saving fails.
    """
    # Ensure directory exists
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    
    if HAS_OPENCV:
        success = cv2.imwrite(path, image)
        if not success:
            raise ImageLoadError(f"Failed to save image: {path}")
    
    elif HAS_PIL:
        try:
            # Convert BGR to RGB if color image
            if len(image.shape) == 3 and image.shape[2] == 3:
                image = image[:, :, ::-1]
            pil_img = Image.fromarray(image)
            pil_img.save(path)
        except Exception as e:
            raise ImageLoadError(f"Failed to save image: {e}")
    
    else:
        raise ImageLoadError(
            "No image library available. Install opencv-python or Pillow."
        )


def warp_image(
    image: np.ndarray,
    matrix_3x3: np.ndarray,
    output_size: Tuple[int, int]
) -> np.ndarray:
    """Apply a 3x3 transformation to an image.
    
    Args:
        image: Input image array.
        matrix_3x3: 3x3 transformation matrix.
        output_size: (height, width) of output image.
        
    Returns:
        Warped image array.
        
    Raises:
        ImageLoadError: If warping fails.
    """
    if not HAS_OPENCV:
        raise ImageLoadError(
            "OpenCV is required for image warping. Install opencv-python."
        )
    
    height, width = output_size
    
    # Use warpAffine for 2x3 matrix (more efficient for rigid/affine)
    # or warpPerspective for full 3x3
    matrix_2x3 = matrix_3x3[:2, :]
    
    try:
        warped = cv2.warpAffine(
            image,
            matrix_2x3,
            (width, height),
            flags=cv2.INTER_LINEAR,
            borderMode=cv2.BORDER_CONSTANT,
            borderValue=0
        )
        return warped
    except Exception as e:
        raise ImageLoadError(f"Failed to warp image: {e}")

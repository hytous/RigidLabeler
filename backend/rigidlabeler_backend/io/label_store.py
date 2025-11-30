"""
Label storage and retrieval.

Handles saving and loading of registration labels in JSON format.
"""

import json
import hashlib
import os
from pathlib import Path
from typing import List, Optional, Dict, Any
from datetime import datetime

from ..config import get_config
from ..api.schemas import (
    Label, TiePoint, Point2D, RigidParams, LabelMeta,
    LabelListItem, LabelSaveResult
)


class LabelStoreError(Exception):
    """Exception for label storage operations."""
    def __init__(self, message: str, error_code: str):
        super().__init__(message)
        self.error_code = error_code


def generate_label_id(image_fixed: str, image_moving: str) -> str:
    """Generate a unique label ID based on image paths.
    
    Uses first 8 characters of MD5 hash of concatenated paths.
    """
    combined = f"{image_fixed}|{image_moving}"
    hash_obj = hashlib.md5(combined.encode('utf-8'))
    return hash_obj.hexdigest()[:8]


def generate_label_filename(image_fixed: str, image_moving: str) -> str:
    """Generate a label filename based on image paths.
    
    Format: {label_id}_{fixed_stem}_{moving_stem}.json
    """
    label_id = generate_label_id(image_fixed, image_moving)
    fixed_stem = Path(image_fixed).stem[:20]  # Limit length
    moving_stem = Path(image_moving).stem[:20]
    
    # Sanitize filenames
    fixed_stem = "".join(c if c.isalnum() or c in "-_" else "_" for c in fixed_stem)
    moving_stem = "".join(c if c.isalnum() or c in "-_" else "_" for c in moving_stem)
    
    return f"{label_id}_{fixed_stem}_{moving_stem}.json"


def save_label(label: Label, labels_root: Optional[str] = None) -> LabelSaveResult:
    """Save a label to the labels directory.
    
    Args:
        label: The Label object to save.
        labels_root: Optional override for labels directory.
        
    Returns:
        LabelSaveResult with path and ID.
        
    Raises:
        LabelStoreError: If save operation fails.
    """
    if labels_root is None:
        config = get_config()
        labels_root = config.paths.labels_root
    
    labels_path = Path(labels_root)
    
    # Ensure directory exists
    try:
        labels_path.mkdir(parents=True, exist_ok=True)
    except OSError as e:
        raise LabelStoreError(
            f"Failed to create labels directory: {e}",
            "IO_ERROR"
        )
    
    # Generate filename and full path
    filename = generate_label_filename(label.image_fixed, label.image_moving)
    label_path = labels_path / filename
    label_id = generate_label_id(label.image_fixed, label.image_moving)
    
    # Convert label to dict
    label_dict = label.model_dump()
    
    # Add timestamp if not present
    if label_dict.get('meta') is None:
        label_dict['meta'] = {}
    if not label_dict['meta'].get('timestamp'):
        label_dict['meta']['timestamp'] = datetime.now().isoformat()
    
    # Write to file
    try:
        with open(label_path, 'w', encoding='utf-8') as f:
            json.dump(label_dict, f, indent=2, ensure_ascii=False)
    except OSError as e:
        raise LabelStoreError(
            f"Failed to save label: {e}",
            "IO_ERROR"
        )
    
    return LabelSaveResult(
        label_path=str(label_path),
        label_id=label_id
    )


def load_label(
    image_fixed: str,
    image_moving: str,
    labels_root: Optional[str] = None
) -> Label:
    """Load a label for a specific image pair.
    
    Args:
        image_fixed: Path to fixed image.
        image_moving: Path to moving image.
        labels_root: Optional override for labels directory.
        
    Returns:
        The loaded Label object.
        
    Raises:
        LabelStoreError: If label not found or read fails.
    """
    if labels_root is None:
        config = get_config()
        labels_root = config.paths.labels_root
    
    labels_path = Path(labels_root)
    filename = generate_label_filename(image_fixed, image_moving)
    label_path = labels_path / filename
    
    if not label_path.exists():
        raise LabelStoreError(
            f"Label not found for given image pair",
            "LABEL_NOT_FOUND"
        )
    
    try:
        with open(label_path, 'r', encoding='utf-8') as f:
            label_dict = json.load(f)
    except json.JSONDecodeError as e:
        raise LabelStoreError(
            f"Invalid JSON in label file: {e}",
            "IO_ERROR"
        )
    except OSError as e:
        raise LabelStoreError(
            f"Failed to read label file: {e}",
            "IO_ERROR"
        )
    
    # Parse into Label object
    try:
        return Label(**label_dict)
    except Exception as e:
        raise LabelStoreError(
            f"Failed to parse label: {e}",
            "IO_ERROR"
        )


def load_label_by_path(label_path: str) -> Label:
    """Load a label from a specific file path.
    
    Args:
        label_path: Full path to the label file.
        
    Returns:
        The loaded Label object.
        
    Raises:
        LabelStoreError: If read fails.
    """
    path = Path(label_path)
    
    if not path.exists():
        raise LabelStoreError(
            f"Label file not found: {label_path}",
            "LABEL_NOT_FOUND"
        )
    
    try:
        with open(path, 'r', encoding='utf-8') as f:
            label_dict = json.load(f)
    except json.JSONDecodeError as e:
        raise LabelStoreError(
            f"Invalid JSON in label file: {e}",
            "IO_ERROR"
        )
    except OSError as e:
        raise LabelStoreError(
            f"Failed to read label file: {e}",
            "IO_ERROR"
        )
    
    try:
        return Label(**label_dict)
    except Exception as e:
        raise LabelStoreError(
            f"Failed to parse label: {e}",
            "IO_ERROR"
        )


def list_labels(labels_root: Optional[str] = None) -> List[LabelListItem]:
    """List all labels in the labels directory.
    
    Args:
        labels_root: Optional override for labels directory.
        
    Returns:
        List of LabelListItem objects.
    """
    if labels_root is None:
        config = get_config()
        labels_root = config.paths.labels_root
    
    labels_path = Path(labels_root)
    
    if not labels_path.exists():
        return []
    
    items = []
    for label_file in labels_path.glob("*.json"):
        try:
            with open(label_file, 'r', encoding='utf-8') as f:
                label_dict = json.load(f)
            
            # Extract label ID from filename
            label_id = label_file.stem.split('_')[0]
            
            items.append(LabelListItem(
                label_id=label_id,
                label_path=str(label_file),
                image_fixed=label_dict.get('image_fixed', ''),
                image_moving=label_dict.get('image_moving', '')
            ))
        except Exception:
            # Skip malformed label files
            continue
    
    return items


def delete_label(
    image_fixed: str,
    image_moving: str,
    labels_root: Optional[str] = None
) -> bool:
    """Delete a label for a specific image pair.
    
    Args:
        image_fixed: Path to fixed image.
        image_moving: Path to moving image.
        labels_root: Optional override for labels directory.
        
    Returns:
        True if deleted, False if not found.
    """
    if labels_root is None:
        config = get_config()
        labels_root = config.paths.labels_root
    
    labels_path = Path(labels_root)
    filename = generate_label_filename(image_fixed, image_moving)
    label_path = labels_path / filename
    
    if label_path.exists():
        try:
            label_path.unlink()
            return True
        except OSError:
            return False
    
    return False

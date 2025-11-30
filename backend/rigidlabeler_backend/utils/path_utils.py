"""
Path utilities for RigidLabeler backend.
"""

import os
from pathlib import Path
from typing import Optional


def normalize_path(path: str) -> str:
    """Normalize a path for cross-platform compatibility.
    
    Args:
        path: Input path string.
        
    Returns:
        Normalized path string.
    """
    return str(Path(path).resolve())


def is_subpath(child: str, parent: str) -> bool:
    """Check if child path is under parent path.
    
    Args:
        child: Potential child path.
        parent: Potential parent path.
        
    Returns:
        True if child is under parent.
    """
    child_path = Path(child).resolve()
    parent_path = Path(parent).resolve()
    
    try:
        child_path.relative_to(parent_path)
        return True
    except ValueError:
        return False


def ensure_dir(path: str) -> Path:
    """Ensure a directory exists, creating if necessary.
    
    Args:
        path: Directory path.
        
    Returns:
        Path object for the directory.
    """
    p = Path(path)
    p.mkdir(parents=True, exist_ok=True)
    return p


def get_relative_path(path: str, base: str) -> str:
    """Get path relative to base directory.
    
    Args:
        path: Full path.
        base: Base directory.
        
    Returns:
        Relative path string, or original if not relative.
    """
    try:
        return str(Path(path).relative_to(Path(base)))
    except ValueError:
        return path

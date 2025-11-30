"""
Configuration management for RigidLabeler backend.

Reads configuration from backend.yaml and provides default values.
"""

import os
from pathlib import Path
from typing import Optional
from dataclasses import dataclass, field
import yaml


def get_project_root() -> Path:
    """Get the project root directory."""
    # Navigate from this file up to project root
    current_file = Path(__file__).resolve()
    # backend/rigidlabeler_backend/config.py -> project root
    return current_file.parent.parent.parent.parent


@dataclass
class ServerConfig:
    """Server configuration."""
    host: str = "127.0.0.1"
    port: int = 8000


@dataclass
class PathsConfig:
    """Paths configuration."""
    data_root: str = "data"
    labels_root: str = "data/labels"
    temp_root: str = "data/temp"
    
    def resolve(self, project_root: Path) -> "PathsConfig":
        """Resolve relative paths against project root."""
        return PathsConfig(
            data_root=str(project_root / self.data_root),
            labels_root=str(project_root / self.labels_root),
            temp_root=str(project_root / self.temp_root)
        )


@dataclass
class LoggingConfig:
    """Logging configuration."""
    level: str = "INFO"


@dataclass
class BackendConfig:
    """Complete backend configuration."""
    server: ServerConfig = field(default_factory=ServerConfig)
    paths: PathsConfig = field(default_factory=PathsConfig)
    logging: LoggingConfig = field(default_factory=LoggingConfig)
    project_root: Path = field(default_factory=get_project_root)
    
    def __post_init__(self):
        """Resolve paths after initialization."""
        self.paths = self.paths.resolve(self.project_root)
        self._ensure_directories()
    
    def _ensure_directories(self):
        """Ensure required directories exist."""
        for path_str in [self.paths.labels_root, self.paths.temp_root]:
            path = Path(path_str)
            if not path.exists():
                path.mkdir(parents=True, exist_ok=True)


def load_config(config_path: Optional[str] = None) -> BackendConfig:
    """Load configuration from YAML file.
    
    Args:
        config_path: Path to backend.yaml. If None, uses default location.
        
    Returns:
        BackendConfig instance with loaded or default values.
    """
    project_root = get_project_root()
    
    if config_path is None:
        config_path = project_root / "config" / "backend.yaml"
    else:
        config_path = Path(config_path)
    
    # Start with default config
    server_cfg = ServerConfig()
    paths_cfg = PathsConfig()
    logging_cfg = LoggingConfig()
    
    # Load from YAML if exists
    if config_path.exists():
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                data = yaml.safe_load(f) or {}
            
            # Parse server config
            if 'server' in data:
                server_data = data['server']
                server_cfg = ServerConfig(
                    host=server_data.get('host', server_cfg.host),
                    port=server_data.get('port', server_cfg.port)
                )
            
            # Parse paths config
            if 'paths' in data:
                paths_data = data['paths']
                paths_cfg = PathsConfig(
                    data_root=paths_data.get('data_root', paths_cfg.data_root),
                    labels_root=paths_data.get('labels_root', paths_cfg.labels_root),
                    temp_root=paths_data.get('temp_root', paths_cfg.temp_root)
                )
            
            # Parse logging config
            if 'logging' in data:
                logging_data = data['logging']
                logging_cfg = LoggingConfig(
                    level=logging_data.get('level', logging_cfg.level)
                )
                
        except Exception as e:
            print(f"Warning: Failed to load config from {config_path}: {e}")
            print("Using default configuration.")
    
    return BackendConfig(
        server=server_cfg,
        paths=paths_cfg,
        logging=logging_cfg,
        project_root=project_root
    )


# Global configuration instance
_config: Optional[BackendConfig] = None


def get_config() -> BackendConfig:
    """Get the global configuration instance.
    
    Loads configuration on first call, then returns cached instance.
    """
    global _config
    if _config is None:
        _config = load_config()
    return _config


def reload_config(config_path: Optional[str] = None) -> BackendConfig:
    """Reload configuration from file.
    
    Args:
        config_path: Optional path to configuration file.
        
    Returns:
        New BackendConfig instance.
    """
    global _config
    _config = load_config(config_path)
    return _config

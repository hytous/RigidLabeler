#!/usr/bin/env python
"""
Run the RigidLabeler backend server.

Usage:
    python run_server.py [--host HOST] [--port PORT] [--reload]
"""

import argparse
import sys
import os
from pathlib import Path

# Fix for PyInstaller --noconsole mode: redirect stdout/stderr if None
if sys.stdout is None:
    sys.stdout = open(os.devnull, 'w')
if sys.stderr is None:
    sys.stderr = open(os.devnull, 'w')

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))


def main():
    parser = argparse.ArgumentParser(description="Run RigidLabeler backend server")
    parser.add_argument(
        "--host",
        type=str,
        default=None,
        help="Host to bind to (default: from config)"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=None,
        help="Port to bind to (default: from config)"
    )
    parser.add_argument(
        "--reload",
        action="store_true",
        help="Enable auto-reload for development"
    )
    parser.add_argument(
        "--config",
        type=str,
        default=None,
        help="Path to backend.yaml config file"
    )
    
    args = parser.parse_args()
    
    # Import after path setup
    from rigidlabeler_backend.config import get_config, reload_config
    
    # Load config
    if args.config:
        config = reload_config(args.config)
    else:
        config = get_config()
    
    # Override with command line args
    host = args.host or config.server.host
    port = args.port or config.server.port
    
    print(f"Starting RigidLabeler backend server...")
    print(f"  Host: {host}")
    print(f"  Port: {port}")
    print(f"  Labels root: {config.paths.labels_root}")
    print(f"  Temp root: {config.paths.temp_root}")
    print()
    
    # Run with uvicorn
    import uvicorn
    uvicorn.run(
        "rigidlabeler_backend.api.server:app",
        host=host,
        port=port,
        reload=args.reload,
        log_level="info"
    )


if __name__ == "__main__":
    main()

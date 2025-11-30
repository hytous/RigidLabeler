# RigidLabeler Windows Installer Build Script
# This script builds the complete Windows installer package

param(
    [string]$QtPath = "D:\Q\Qt\Qt\6.7.3\mingw_64",
    [string]$BuildType = "Release",
    [switch]$SkipFrontend,
    [switch]$SkipBackend,
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$PackagingDir = $ScriptDir
$DistDir = Join-Path $PackagingDir "dist"
$FrontendDist = Join-Path $DistDir "frontend"
$BackendDist = Join-Path $DistDir "backend"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RigidLabeler Installer Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Project Root: $ProjectRoot"
Write-Host "Distribution Dir: $DistDir"
Write-Host ""

# Clean previous build
if (Test-Path $DistDir) {
    Write-Host "Cleaning previous build..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $DistDir
}
New-Item -ItemType Directory -Path $DistDir -Force | Out-Null
New-Item -ItemType Directory -Path $FrontendDist -Force | Out-Null
New-Item -ItemType Directory -Path $BackendDist -Force | Out-Null

# ============================================================================
# Step 1: Build and Deploy Qt Frontend
# ============================================================================
if (-not $SkipFrontend) {
    Write-Host ""
    Write-Host "Step 1: Building Qt Frontend..." -ForegroundColor Green
    Write-Host "----------------------------------------"
    
    $FrontendSrc = Join-Path $ProjectRoot "frontend"
    $FrontendBuildDir = Join-Path $FrontendSrc "build\Desktop_Qt_6_7_3_MinGW_64_bit-Release"
    
    # Check if release build exists
    $FrontendExe = Join-Path $FrontendBuildDir "release\frontend.exe"
    if (-not (Test-Path $FrontendExe)) {
        # Try alternative path
        $FrontendExe = Join-Path $FrontendBuildDir "frontend.exe"
    }
    
    if (-not (Test-Path $FrontendExe)) {
        Write-Host "ERROR: Frontend executable not found!" -ForegroundColor Red
        Write-Host "Please build the frontend in Release mode first using Qt Creator." -ForegroundColor Red
        Write-Host "Expected path: $FrontendExe" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "Found frontend executable: $FrontendExe"
    
    # Copy executable
    Copy-Item $FrontendExe -Destination $FrontendDist
    
    # Run windeployqt
    $WinDeployQt = Join-Path $QtPath "bin\windeployqt.exe"
    if (-not (Test-Path $WinDeployQt)) {
        Write-Host "ERROR: windeployqt not found at $WinDeployQt" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "Running windeployqt..."
    $TargetExe = Join-Path $FrontendDist "frontend.exe"
    & $WinDeployQt --release --no-translations --no-system-d3d-compiler --no-opengl-sw $TargetExe
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: windeployqt failed!" -ForegroundColor Red
        exit 1
    }
    
    # Copy translations
    $TranslationsDir = Join-Path $FrontendDist "translations"
    if (-not (Test-Path $TranslationsDir)) {
        New-Item -ItemType Directory -Path $TranslationsDir -Force | Out-Null
    }
    
    # Copy our custom translations (compiled .qm files)
    $QmFiles = Get-ChildItem -Path (Join-Path $FrontendSrc "translations") -Filter "*.qm" -ErrorAction SilentlyContinue
    foreach ($qm in $QmFiles) {
        Copy-Item $qm.FullName -Destination $TranslationsDir
    }
    
    Write-Host "Frontend deployment complete!" -ForegroundColor Green
}

# ============================================================================
# Step 2: Package Python Backend with PyInstaller
# ============================================================================
if (-not $SkipBackend) {
    Write-Host ""
    Write-Host "Step 2: Packaging Python Backend..." -ForegroundColor Green
    Write-Host "----------------------------------------"
    
    $BackendSrc = Join-Path $ProjectRoot "backend"
    $PyInstallerSpec = Join-Path $PackagingDir "backend.spec"
    
    # Check if PyInstaller is installed
    $PyInstallerCheck = python -c "import PyInstaller" 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Installing PyInstaller..." -ForegroundColor Yellow
        pip install pyinstaller
    }
    
    # Run PyInstaller
    Write-Host "Running PyInstaller..."
    Push-Location $BackendSrc
    
    $BackendPackage = Join-Path $BackendSrc "rigidlabeler_backend"
    
    python -m PyInstaller `
        --name "rigidlabeler_backend" `
        --onedir `
        --noconsole `
        --hidden-import "uvicorn.logging" `
        --hidden-import "uvicorn.loops" `
        --hidden-import "uvicorn.loops.auto" `
        --hidden-import "uvicorn.protocols" `
        --hidden-import "uvicorn.protocols.http" `
        --hidden-import "uvicorn.protocols.http.auto" `
        --hidden-import "uvicorn.protocols.websockets" `
        --hidden-import "uvicorn.protocols.websockets.auto" `
        --hidden-import "uvicorn.lifespan" `
        --hidden-import "uvicorn.lifespan.on" `
        --hidden-import "fastapi" `
        --hidden-import "starlette" `
        --hidden-import "anyio" `
        --hidden-import "numpy" `
        --hidden-import "PIL" `
        --hidden-import "torch" `
        --hidden-import "yaml" `
        --hidden-import "pydantic" `
        --collect-all "numpy" `
        --collect-all "fastapi" `
        --collect-all "starlette" `
        --add-data "$BackendPackage;rigidlabeler_backend" `
        --distpath "$BackendDist" `
        --workpath "$PackagingDir\build" `
        --specpath "$PackagingDir" `
        --clean `
        -y `
        "scripts\run_server.py"
    
    Pop-Location
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: PyInstaller failed!" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "Backend packaging complete!" -ForegroundColor Green
}

# ============================================================================
# Step 3: Copy Config Files
# ============================================================================
Write-Host ""
Write-Host "Step 3: Copying configuration files..." -ForegroundColor Green
Write-Host "----------------------------------------"

$ConfigSrc = Join-Path $ProjectRoot "config"
$ConfigDist = Join-Path $DistDir "config"

if (Test-Path $ConfigSrc) {
    Copy-Item -Recurse $ConfigSrc -Destination $ConfigDist
    Write-Host "Configuration files copied."
} else {
    New-Item -ItemType Directory -Path $ConfigDist -Force | Out-Null
    Write-Host "Created empty config directory."
}

# ============================================================================
# Step 4: Create Launcher Script
# ============================================================================
Write-Host ""
Write-Host "Step 4: Creating launcher..." -ForegroundColor Green
Write-Host "----------------------------------------"

# Create a launcher that starts both frontend and backend
$LauncherContent = @'
@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

:: Start backend in background
start "" /B "%SCRIPT_DIR%backend\rigidlabeler_backend\rigidlabeler_backend.exe"

:: Wait a moment for backend to start
timeout /t 2 /nobreak > nul

:: Start frontend
start "" "%SCRIPT_DIR%frontend\frontend.exe"

exit
'@

$LauncherPath = Join-Path $DistDir "RigidLabeler.bat"
Set-Content -Path $LauncherPath -Value $LauncherContent -Encoding ASCII

Write-Host "Launcher created."

# ============================================================================
# Step 5: Build Installer with Inno Setup
# ============================================================================
if (-not $SkipInstaller) {
    Write-Host ""
    Write-Host "Step 5: Building Installer..." -ForegroundColor Green
    Write-Host "----------------------------------------"
    
    $InnoSetupCompiler = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    if (-not (Test-Path $InnoSetupCompiler)) {
        # Try alternative path
        $InnoSetupCompiler = "C:\Program Files\Inno Setup 6\ISCC.exe"
    }
    
    if (-not (Test-Path $InnoSetupCompiler)) {
        Write-Host "WARNING: Inno Setup not found!" -ForegroundColor Yellow
        Write-Host "Please install Inno Setup 6 from: https://jrsoftware.org/isdl.php" -ForegroundColor Yellow
        Write-Host "Then run this script again, or manually compile: $PackagingDir\installer.iss" -ForegroundColor Yellow
    } else {
        $IssFile = Join-Path $PackagingDir "installer.iss"
        & $InnoSetupCompiler $IssFile
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Installer created successfully!" -ForegroundColor Green
            Write-Host "Output: $PackagingDir\Output\RigidLabeler_Setup.exe" -ForegroundColor Cyan
        } else {
            Write-Host "ERROR: Inno Setup compilation failed!" -ForegroundColor Red
        }
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build Complete!" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Distribution files are in: $DistDir"

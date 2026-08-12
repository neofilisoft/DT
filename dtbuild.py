#!/usr/bin/env python3
import argparse
import subprocess
import os
import shutil
import sys
import platform

# Configuration
ENGINE_ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR_BASE = os.path.join(ENGINE_ROOT, 'build')
DIST_DIR = os.path.join(ENGINE_ROOT, 'dist')
GAME_NAME = 'Domaintic'
GAME_EXE = f"{GAME_NAME}.exe" if platform.system() == 'Windows' else GAME_NAME

def run_command(cmd, cwd=None):
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        print(f"Command failed with exit code {result.returncode}")
        sys.exit(result.returncode)

def get_build_dir(config):
    # Use 'build' to match the existing cmake setup
    return BUILD_DIR_BASE

def configure(config):
    build_dir = get_build_dir(config)
    os.makedirs(build_dir, exist_ok=True)
    cmd = ['cmake', '..', f'-DCMAKE_BUILD_TYPE={config}', '-G', 'Ninja']
    run_command(cmd, cwd=build_dir)

def build(config):
    configure(config)
    build_dir = get_build_dir(config)
    cmd = ['cmake', '--build', '.', '--config', config]
    run_command(cmd, cwd=build_dir)

def run(config):
    build(config)
    build_dir = get_build_dir(config)
    
    # Path to the game executable
    exe_path = os.path.join(build_dir, 'source', 'game', GAME_EXE)
    if not os.path.exists(exe_path):
        print(f"Executable not found at {exe_path}")
        sys.exit(1)
        
    print(f"Starting {GAME_NAME}...")
    run_command([exe_path], cwd=ENGINE_ROOT)

def clean():
    if os.path.exists(BUILD_DIR_BASE):
        print(f"Removing {BUILD_DIR_BASE}...")
        shutil.rmtree(BUILD_DIR_BASE)
    if os.path.exists(DIST_DIR):
        print(f"Removing {DIST_DIR}...")
        shutil.rmtree(DIST_DIR)
    print("Clean complete.")

def package(target_platform):
    config = 'Release'
    print(f"Packaging for {target_platform} in {config} mode...")
    build(config)
    
    build_dir = get_build_dir(config)
    exe_path = os.path.join(build_dir, 'source', 'game', GAME_EXE)
    if not os.path.exists(exe_path):
        print(f"Executable not found at {exe_path}")
        sys.exit(1)
        
    # Create dist folder
    package_name = f"{GAME_NAME}-{target_platform}"
    package_dir = os.path.join(DIST_DIR, package_name)
    
    if os.path.exists(package_dir):
        shutil.rmtree(package_dir)
    os.makedirs(package_dir)
    
    # Copy executable
    print(f"Copying executable to {package_dir}")
    shutil.copy2(exe_path, package_dir)
    
    # Copy assets to the hardcoded path the engine currently expects
    # In the future, this should be abstracted by a virtual file system (VFS)
    assets_src = os.path.join(ENGINE_ROOT, 'source', 'engine', 'asset')
    if os.path.exists(assets_src):
        assets_dest = os.path.join(package_dir, 'source', 'engine', 'asset')
        os.makedirs(os.path.dirname(assets_dest), exist_ok=True)
        print(f"Copying assets to {assets_dest}")
        shutil.copytree(assets_src, assets_dest)
    else:
        print(f"Warning: assets folder not found at {assets_src}, skipping.")
        
    # Copy dynamic libraries (DLLs)
    # The current build system copies SDL3.dll to the build bin directory or it's static?
    # Let's check if there is an SDL3.dll in the system path or build tree
    # If using dynamic linking on Windows for SDL3, it might be in thirdparty/sdl or installed globally.
    # We will look for SDL3.dll in the build directory, if any.
    sdl_dll = os.path.join(build_dir, 'thirdparty', 'sdl', 'SDL3.dll')
    if os.path.exists(sdl_dll):
        print(f"Copying SDL3.dll to {package_dir}")
        shutil.copy2(sdl_dll, package_dir)
    else:
        # Also check root if CMake copies it there
        sdl_dll_root = os.path.join(ENGINE_ROOT, 'SDL3.dll')
        if os.path.exists(sdl_dll_root):
            print(f"Copying SDL3.dll to {package_dir}")
            shutil.copy2(sdl_dll_root, package_dir)
        else:
            print(f"Note: SDL3.dll not found, assuming static linkage or system-provided.")

    print(f"Packaging complete! Your game is ready at: {package_dir}")
    
    # Optional: Zip it up
    zip_path = os.path.join(DIST_DIR, package_name)
    shutil.make_archive(zip_path, 'zip', DIST_DIR, package_name)
    print(f"Created archive at: {zip_path}.zip")

def main():
    parser = argparse.ArgumentParser(description="DTEngine Build and Packaging Tool")
    subparsers = parser.add_subparsers(dest="command", help="Commands")
    
    # Build command
    build_parser = subparsers.add_parser("build", help="Build the project")
    build_parser.add_argument("--config", choices=["Debug", "Release"], default="Debug", help="Build configuration")
    
    # Run command
    run_parser = subparsers.add_parser("run", help="Build and run the game")
    run_parser.add_argument("--config", choices=["Debug", "Release"], default="Debug", help="Build configuration")
    
    # Package command
    package_parser = subparsers.add_parser("package", help="Package the game for distribution")
    package_parser.add_argument("--platform", choices=["windows", "linux", "mac"], default="windows", help="Target platform")
    
    # Clean command
    clean_parser = subparsers.add_parser("clean", help="Clean build directories")
    
    args = parser.parse_args()
    
    if args.command == "build":
        build(args.config)
    elif args.command == "run":
        run(args.config)
    elif args.command == "package":
        package(args.platform)
    elif args.command == "clean":
        clean()
    else:
        parser.print_help()

if __name__ == "__main__":
    main()

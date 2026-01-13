#!/bin/bash
set -e

# Ensure we are in the project root
cd "$(dirname "$0")"

ZMK_IMAGE="zmkfirmware/zmk-build-arm:stable"

echo "=== ZMK Local Build Script ==="
echo "Using Docker Image: $ZMK_IMAGE"
echo "Parsing build.yaml..."
echo "=============================="

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
    echo "Error: Docker is not running. Please start Docker."
    exit 1
fi

# Run the build process in a container
docker run --rm \
    -v "$(pwd):/workspace" \
    -w /workspace \
    "$ZMK_IMAGE" \
    /bin/bash -c "
    
    # 1. Initialize West if needed
    if [ ! -d \".west\" ]; then
        echo \"Initializing west workspace...\"
        west init -l config
    fi

    # 2. Update modules
    echo \"Updating modules...\"
    west update

    # 3. Export Zephyr CMake package
    echo \"Exporting Zephyr CMake config...\"
    west zephyr-export

    # 4. Create and run Python builder script
    cat <<EOF > builder.py
import yaml
import subprocess
import os
import sys
import shlex

def build_target(entry):
    board = entry.get('board')
    shield = entry.get('shield')
    artifact_name = entry.get('artifact-name')
    cmake_args = entry.get('cmake-args', '')
    snippet = entry.get('snippet', '')
    
    print(f'\\n=== Building: {artifact_name} ({board} / {shield}) ===')
    
    cmd = ['west', 'build', '-p', '-s', 'zmk/app', '-b', board]
    
    extra_args = ['-DZMK_CONFIG=/workspace/config', '-DBOARD_ROOT=/workspace']
    
    if shield:
        extra_args.append(f'-DSHIELD={shield}')
    
    if snippet:
        extra_args.append(f'-DSNIPPET={snippet}')
        
    if cmake_args:
        # Split cmake args string into list
        extra_args.extend(cmake_args.split())
        
    cmd.append('--')
    cmd.extend(extra_args)
    
    cmd_str = ' '.join(shlex.quote(s) for s in cmd)
    print(f'Command: {cmd_str}')
    
    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError:
        print(f'Error building {artifact_name}')
        sys.exit(1)
        
    # Copy artifact
    if artifact_name:
        src = 'build/zephyr/zmk.uf2'
        dst = f'artifacts/{artifact_name}.uf2'
        os.makedirs('artifacts', exist_ok=True)
        if os.path.exists(src):
            subprocess.run(['cp', src, dst], check=True)
            print(f'Artifact saved to: {dst}')
        else:
            print(f'Warning: {src} not found')

if __name__ == '__main__':
    try:
        if not os.path.exists('build.yaml'):
             print('Error: build.yaml not found')
             sys.exit(1)

        with open('build.yaml', 'r') as f:
            config = yaml.safe_load(f)
            
        include_list = config.get('include', [])
        if not include_list:
            print('No builds found in build.yaml')
            # Fallback to simple list if no include list (older format or empty)
            # But the user file has include list, so this is fine.
            sys.exit(0)
            
        for entry in include_list:
            if entry is None: continue 
            # Check if entry is commented out or empty
            build_target(entry)
            
    except Exception as e:
        print(f'Error processing build.yaml: {e}')
        sys.exit(1)
EOF

    python3 builder.py
    rm builder.py
    "

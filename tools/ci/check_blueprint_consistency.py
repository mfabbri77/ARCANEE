#!/usr/bin/env python3
import sys
import os

def check_blueprint_consistency():
    # Placeholder for MS-01: strict implementation comes later or iteratively.
    # Currently checks that blueprint directory exists and has files.
    
    if len(sys.argv) > 1:
        root_dir = sys.argv[1]
    else:
        root_dir = os.getcwd()

    blueprint_dir = os.path.join(root_dir, 'blueprint')
    if not os.path.isdir(blueprint_dir):
        print(f"::error::Blueprint directory not found at {blueprint_dir}")
        sys.exit(1)

    # Basic check: do we have the main chapters?
    required_files = [f'blueprint_v0.1_ch{i}.md' for i in range(10)]
    missing = []
    
    for f in required_files:
        if not os.path.exists(os.path.join(blueprint_dir, f)):
            missing.append(f)
            
    if missing:
        print(f"::error::Missing blueprint chapters: {missing}")
        sys.exit(1)

    print("SUCCESS: Blueprint structural consistency check passed (Placeholder MS-01).")
    sys.exit(0)

if __name__ == '__main__':
    check_blueprint_consistency()

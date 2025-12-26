#!/usr/bin/env python3
import os
import sys

def check_no_temp_dbg():
    if len(sys.argv) > 1:
        root_dir = sys.argv[1]
    else:
        root_dir = os.getcwd()
    
    print(f"Scanning directory: {root_dir}")

    # Extensions to check
    extensions = ('.cpp', '.h', '.hpp', '.c', '.cmake', '.txt', '.md', '.py', '.sh')
    # Directories to exclude
    exclude_dirs = {'.git', 'build', 'install', 'third_party', 'external', '.vscode', '_blueprint'}
    
    found_errors = False

    for dirpath, dirnames, filenames in os.walk(root_dir):
        # Filter excluded dirs in-place
        dirnames[:] = [d for d in dirnames if d not in exclude_dirs]
        
        for filename in filenames:
            if not filename.endswith(extensions) and filename != 'CMakeLists.txt':
                continue
                
            filepath = os.path.join(dirpath, filename)
            
            # Skip this script itself
            if os.path.abspath(filepath) == os.path.abspath(__file__):
                continue
                
            try:
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    for line_num, line in enumerate(f, 1):
                        if '[TEMP-DBG]' in line:
                            print(f"::error file={filepath},line={line_num}::Found [TEMP-DBG] marker: {line.strip()}")
                            found_errors = True
            except Exception as e:
                print(f"Warning: Could not read {filepath}: {e}")

    if found_errors:
        sys.exit(1)
    
    print("SUCCESS: No [TEMP-DBG] markers found.")
    sys.exit(0)

if __name__ == '__main__':
    check_no_temp_dbg()

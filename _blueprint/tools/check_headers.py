#!/usr/bin/env python3
import os
import sys

def check_headers(root_dir):
    failed = False
    for root, dirs, files in os.walk(root_dir):
        for name in files:
            if not name.endswith(".md"):
                continue
                
            file_path = os.path.join(root, name)
            rel_path = os.path.relpath(file_path, os.getcwd())
            expected = f"<!-- {rel_path} -->"
            
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    first_line = f.readline().strip()
                
                if first_line != expected:
                    print(f"Header mismatch in {rel_path}")
                    print(f"  Expected: {expected}")
                    print(f"  Found:    {first_line}")
                    failed = True
            except Exception as e:
                print(f"Error reading {rel_path}: {e}")
                failed = True
                
    return not failed

if __name__ == "__main__":
    if not check_headers("_blueprint"):
        sys.exit(1)
    print("Markdown headers: OK")
    sys.exit(0)

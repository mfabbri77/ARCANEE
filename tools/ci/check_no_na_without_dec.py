#!/usr/bin/env python3
import sys
import glob
import os
import re

def main():
    if len(sys.argv) > 1:
        root = sys.argv[1]
    else:
        root = os.getcwd()
    
    blueprint_dir = os.path.join(root, '_blueprint')
    # Scan project/ and existing v* directories
    files = glob.glob(os.path.join(blueprint_dir, 'project', '*.md')) + \
            glob.glob(os.path.join(blueprint_dir, 'v*', '*.md')) + \
            glob.glob(os.path.join(blueprint_dir, '*.md'))
    
    errors = 0
    
    # Regex for N/A
    # We look for "N/A" bounding word boundaries or distinct usage
    # But simply "N/A" is usually valid.
    
    for f_path in files:
        with open(f_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
            for idx, line in enumerate(lines):
                if 'N/A' in line:
                    # Ignore if line contains [TEST-02] (self-doc) or is code
                    if '[TEST-02]' in line or '`N/A`' in line:
                        continue
                    # Check for [DEC-XX]
                    if not re.search(r'\[DEC-\d+\]', line):
                        # Allow if it's "inherited from" line? No, "inherit from" doesn't use N/A.
                        # Allow if explicitly excluded? 
                        print(f"::error file={f_path},line={idx+1}::Found N/A without [DEC-XX] justification: {line.strip()}")
                        errors += 1
                        
    if errors > 0:
        sys.exit(1)
        
    print("SUCCESS: No unjustified N/A markers found.")

if __name__ == '__main__':
    main()

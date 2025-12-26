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
    files = glob.glob(os.path.join(blueprint_dir, 'project', '*.md')) + \
            glob.glob(os.path.join(blueprint_dir, 'v*', '*.md')) + \
            glob.glob(os.path.join(blueprint_dir, '*.md'))
    
    errors = 0
    
    for f_path in files:
        with open(f_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
            for idx, line in enumerate(lines):
                if '[TEST-04]' in line:
                    continue
                # pattern: inherit from /blueprint/filename.md or filename
                # Capture everything until space or end of line, avoiding closing punctuation like ”
                m = re.search(r'inherit from\s+([^\s”"]+)', line)
                if m:
                    target = m.group(1).rstrip('.”",')
                    if target in ['…', '...']:
                        continue
                    # Resolve path
                    if target.startswith('/'):
                        # Absolute from repo root
                        target_path = os.path.join(root, target.lstrip('/'))
                    else:
                        target_path = os.path.join(root, target)
                    
                    if not os.path.exists(target_path):
                        print(f"::error file={f_path},line={idx+1}::Inheritance target not found: {target} (resolved to {target_path})")
                        errors += 1

    if errors > 0:
        sys.exit(1)

    print("SUCCESS: All inheritance targets exist.")

if __name__ == '__main__':
    main()

#!/usr/bin/env python3
import os
import sys

def fix_headers(root_dir):
    check_only = False
    fixed_count = 0
    
    for root, dirs, files in os.walk(root_dir):
        for name in files:
            if not name.endswith(".md"):
                continue
                
            file_path = os.path.join(root, name)
            rel_path = os.path.relpath(file_path, os.getcwd())
            expected = f"<!-- {rel_path} -->"
            
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                lines = content.splitlines()
                if not lines:
                    # Empty file
                    new_content = expected + "\n"
                    with open(file_path, 'w', encoding='utf-8') as f:
                        f.write(new_content)
                    print(f"Fixed: {rel_path}")
                    fixed_count += 1
                    continue
                    
                first_line = lines[0].strip()
                if first_line != expected:
                    # Check if it's an old header mismatch or missing
                    if first_line.startswith("<!--") and first_line.endswith("-->"):
                        # Replace existing bad header
                        lines[0] = expected
                    else:
                        # Prepend new header
                        lines.insert(0, expected)
                        # Add blank line after header if not present? Spec says "First line MUST be".
                        # Usually good style to have a newline after.
                        if len(lines) > 1 and lines[1].strip() != "":
                            lines.insert(1, "")
                            
                    with open(file_path, 'w', encoding='utf-8') as f:
                        f.write("\n".join(lines) + "\n")
                    print(f"Fixed: {rel_path}")
                    fixed_count += 1
                    
            except Exception as e:
                print(f"Error processing {rel_path}: {e}")

    print(f"Total files fixed: {fixed_count}")

if __name__ == "__main__":
    fix_headers("_blueprint")

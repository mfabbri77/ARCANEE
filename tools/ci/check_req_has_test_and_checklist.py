#!/usr/bin/env python3
import sys
import glob
import os
import re

def parse_ids(text):
    # Matches [REQ-123] or REQ-123 (without brackets)
    res = set()
    # Single IDs. Allow optional brackets.
    for m in re.finditer(r'\[?REQ-(\d+)\]?', text):
        res.add(int(m.group(1)))
    # Ranges: [REQ-123..125] or REQ-123..125
    for m in re.finditer(r'\[?REQ-(\d+)\.\.(\d+)\]?', text):
        start, end = int(m.group(1)), int(m.group(2))
        for i in range(start, end + 1):
            res.add(i)
    return res

def main():
    if len(sys.argv) > 1:
        root = sys.argv[1]
    else:
        root = os.getcwd()

    blueprint_dir = os.path.join(root, 'blueprint')
    checklist_path = os.path.join(blueprint_dir, 'implementation_checklist.yaml')
    
    # 1. Gather all defined REQs from blueprint
    defined_reqs = set()
    files = glob.glob(os.path.join(blueprint_dir, '*.md'))
    for f_path in files:
        with open(f_path, 'r', encoding='utf-8') as f:
             # loose check: any mention might be a definition or ref.
             # We assume if it appears in MD it exists.
             defined_reqs.update(parse_ids(f.read()))

    if not defined_reqs:
        # Fallback if no REQs found (unlikely)
        print("No REQs found in blueprint.")
        sys.exit(0)

    # 2. Gather covered REQs from checklist
    covered_reqs = set()
    if os.path.exists(checklist_path):
        with open(checklist_path, 'r', encoding='utf-8') as f:
            covered_reqs.update(parse_ids(f.read()))
    else:
        print("::error::implementation_checklist.yaml not found!")
        sys.exit(1)

    # 3. Check diff
    missing = defined_reqs - covered_reqs
    if missing:
        # Filter out REQs that might be "future" or "out of scope" if marked?
        # For now, strict:
        # Sort for display
        missing_list = sorted(list(missing))
        print(f"::error::Found {len(missing)} [REQ] IDs in Blueprint not referenced in Checklist: {[ f'REQ-{x}' for x in missing_list ]}")
        # Soft failure or hard? Hard per blueprint.
        sys.exit(1)

    print(f"SUCCESS: All {len(defined_reqs)} observed REQs are referenced in checklist.")

if __name__ == '__main__':
    main()

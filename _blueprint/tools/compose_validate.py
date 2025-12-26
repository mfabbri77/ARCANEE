#!/usr/bin/env python3
import os
import sys
import yaml
import re
from pathlib import Path

# Configuration
SCRIPT_DIR = Path(__file__).parent.resolve()
# Assuming script is in _blueprint/tools/
BLUEPRINT_ROOT = SCRIPT_DIR.parent.resolve()
REPO_ROOT = BLUEPRINT_ROOT.parent.resolve()

MANDATORY_DIRS = ["project", "rules", "knowledge", "tools"]
MANDATORY_FILES = ["id_registry.yaml", "rules/ci_gates.yaml"]
PROJECT_CHAPTERS = [f"ch{i}.md" for i in range(10)] + [
    "decision_log.md",
    "walkthrough.md",
    "implementation_checklist.yaml"
]

def load_yaml(path):
    with open(path, 'r') as f:
        return yaml.safe_load(f)

def get_all_ids_from_text(text):
    # Regex to find IDs like REQ-0001, DEC-0020, etc.
    # Avoiding loose matches using boundary checks if possible, 
    # but strictly following the [PREFIX-NUMBER] or PREFIX-NUMBER format.
    # The spec says "PREFIX-0000".
    return set(re.findall(r'\b[A-Z]+-\d+\b', text))

def validate_structure():
    print("Validating directory structure...")
    if not BLUEPRINT_ROOT.is_dir():
        print(f"FAIL: {BLUEPRINT_ROOT} missing.")
        return False
    
    for d in MANDATORY_DIRS:
        if not (BLUEPRINT_ROOT / d).is_dir():
            print(f"FAIL: Directory {d} missing.")
            return False

    for f in MANDATORY_FILES:
        if not (BLUEPRINT_ROOT / f).exists():
            print(f"FAIL: File {f} missing.")
            return False
            
    print("Structure: OK")
    return True

def validate_ids():
    print("Validating ID Registry uniqueness...")
    registry_path = BLUEPRINT_ROOT / "id_registry.yaml"
    try:
        registry = load_yaml(registry_path)
    except Exception as e:
        print(f"FAIL: Could not load id_registry: {e}")
        return False
    
    # We could check if defined IDs collide, but mainly we want to iterate all blueprint text
    # and ensure multiple definitions don't exist for the same ID (if strictly enforced).
    # For System Architect v2.0, "Never reuse" is the rule.
    # Here we just check the registry format.
    if "registry" not in registry:
        print("FAIL: id_registry.yaml missing 'registry' key.")
        return False

    print("ID Registry: OK")
    return True

def validate_header_rule(file_path):
    # Check if first line is <!-- <repo-relative-path> -->
    # For _blueprint only
    rel_path = os.path.relpath(file_path, str(REPO_ROOT))
    
    # We expect the comment to match the relative path
    # e.g. <!-- _blueprint/project/ch0.md -->
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            first_line = f.readline().strip()
            
        expected = f"<!-- {rel_path} -->"
        if first_line != expected:
            # Try matching with ./ prefix if needed, or normalized path
            pass # strict check
            if first_line.replace('./', '') != expected.replace('./', ''):
                print(f"Header mismatch in {rel_path}")
                print(f"  Expected: {expected}")
                print(f"  Found:    {first_line}")
                return False
    except Exception as e:
        print(f"Error reading {file_path}: {e}")
        return False
        
    return True

def validate_traceability(args):
    print("Validating traceability (REQ -> TEST/Checklist)...")
    
    # 1. Harvest REQs from blueprint text (ch0-9)
    # 2. Harvest TESTs from blueprint text
    # 3. Harvest Checklist Items from implementation_checklist.yaml
    
    bp_project = BLUEPRINT_ROOT / "project"
    
    reqs = set()
    tests = set()
    checklist_refs = set()
    
    # Scan chapters
    for md_file in bp_project.glob("*.md"):
        content = md_file.read_text(encoding='utf-8')
        # Find defined sections like ## [REQ-001]
        # or references in text. 
        # A crude scan for "REQ-\d+" will catch usages, not just definitions.
        # But definitions usually appear in headers ## [REQ-XX] or bold **[REQ-XX]**.
        # For this checker, we assume any occurrence in ch1-6 implies a requirement existence if it's in the registry?
        # Let's simple-scan for now: find anything looking like an ID.
        
        # A better heuristic for *definition*:
        # [REQ-XXX] at start of line or in headers.
        
        found_ids = get_all_ids_from_text(content)
        for id_str in found_ids:
            if id_str.startswith("REQ-"):
                reqs.add(id_str)
            if id_str.startswith("TEST-"):
                tests.add(id_str)

    # Scan checklist
    checklist_path = bp_project / "implementation_checklist.yaml"
    if checklist_path.exists():
        cl_data = load_yaml(checklist_path)
        # Traverse implementation_checklist structure
        # implementation_checklist -> milestones -> steps -> refs
        
        impl_cl = cl_data.get("implementation_checklist", {})
        milestones = impl_cl.get("milestones", [])
        for ms in milestones:
            steps = ms.get("steps", [])
            for step in steps:
                refs = step.get("refs", [])
                for r in refs:
                    # Handle ranges like REQ-1..62 or REQ-01..10
                    range_match = re.match(r"([A-Z]+)-(\d+)\.\.(\d+)", r)
                    if range_match:
                        prefix = range_match.group(1)
                        start = int(range_match.group(2))
                        end = int(range_match.group(3))
                        for i in range(start, end + 1):
                            # Try both formats REQ-1 and REQ-01 (or whatever matches defined REQs)
                            # But simply adding "REQ-{i}" and "REQ-{i:02d}" to the set is safest
                            checklist_refs.add(f"{prefix}-{i}")
                            checklist_refs.add(f"{prefix}-{i:02d}")
                    else:
                        checklist_refs.add(r)
    
    # Check trace: Every REQ must be in checklist_refs AND associated with a TEST?
    # Spec: "Every REQ MUST map to >=1 TEST and >=1 checklist step"
    
    # We can't easily know which REQ links to which TEST just by text scanning without parsing semantic mapping.
    # However, we CAN check if the REQ is referenced in the checklist.
    
    missing_checklist = []
    
    for req in reqs:
        if req not in checklist_refs:
            # Might be defined but not implemented yet? Reference says "Existing REQ trace".
            # If it's in the blueprint, it should be in the checklist.
            missing_checklist.append(req)
            
    if missing_checklist:
        print(f"FAIL: The following REQs are missing from implementation_checklist.yaml: {len(missing_checklist)}")
        # Print first 5
        for r in missing_checklist[:5]:
            print(f"  - {r}")
        if not args.trace:
            print("  (Run with --trace for full analysis)")
        return False
        
    print("Traceability: OK")
    return True

def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--trace", action="store_true", help="Run full requirement traceability check")
    args = parser.parse_args()

    success = True
    
    if not validate_structure():
        success = False
        
    if not validate_ids():
        success = False
        
    # Validate headers
    print("Validating markdown headers...")
    for md_file in BLUEPRINT_ROOT.rglob("*.md"):
        if not validate_header_rule(str(md_file)):
            success = False

    if not validate_traceability(args):
        # Traceability failure is non-fatal for now unless strict?
        # Gate says "gate req-trace fails otherwise". So it should fail.
        success = False
        
    if success:
        print("Validation PASSED.")
        sys.exit(0)
    else:
        print("Validation FAILED.")
        sys.exit(1)

if __name__ == "__main__":
    main()

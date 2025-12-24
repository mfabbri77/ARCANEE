
import subprocess
import sys
import os

def main():
    bin_path = os.path.join("build/dev/bin", "arcanee")
    
    print(f"Running benchmark: {bin_path} --benchmark")
    
    try:
        # Run process
        result = subprocess.run(
            [bin_path, "--benchmark"], 
            capture_output=True, 
            text=True, 
            timeout=60 # Increased timeout
        )
        
        if result.returncode != 0:
            print("Process failed with return code", result.returncode)
            print("Stdout:", result.stdout)
            print("Stderr:", result.stderr)
            return 1
            
        print("Output:\n", result.stdout)
        
        # Parse Output
        fps = 0.0
        for line in result.stdout.splitlines():
            if line.startswith("BENCHMARK_RESULT,"):
                parts = line.split(",")
                fps = float(parts[3])
                print(f"Captured FPS: {fps}")
                break
                
        if fps < 30.0:
            print(f"FAIL: FPS {fps} is below threshold 30.0")
            return 1
            
        print("PASS: Benchmark completed successfully.")
        return 0

    except Exception as e:
        print(f"Test execution failed: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())

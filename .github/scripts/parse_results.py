#!/usr/bin/env python3
import os
import json
import glob
import sys

output_dir = sys.argv[1] if len(sys.argv) > 1 else "results"
category = sys.argv[2] if len(sys.argv) > 2 else "unknown"

results = []
total_time = 0.0
total_cost = 0
feasible_count = 0
error_count = 0

out_files = sorted(glob.glob(os.path.join(output_dir, "*.out")))
if not out_files:
    print(f"Warning: No output files found in {output_dir}")
else:
    for out_file in out_files:
        instance_name = os.path.basename(out_file)[:-4]
        try:
            with open(out_file, 'r') as f:
                output = f.read().strip()
            if not output:
                error_count += 1
                results.append({"instance": instance_name, "status": "error", "error": "Empty output"})
                continue
            parts = output.split()
            if len(parts) < 4:
                error_count += 1
                results.append({"instance": instance_name, "status": "error", "error": "Invalid output format"})
                continue
            num_packages = int(parts[0])
            total_cost_inst = int(parts[1])
            is_feasible = int(parts[2])
            time_ms = float(parts[-1])
            solution = " ".join(parts[3:-1])
            if is_feasible:
                feasible_count += 1
            total_time += time_ms
            total_cost += total_cost_inst
            results.append({
                "instance": instance_name,
                "status": "success",
                "num_packages": num_packages,
                "total_cost": total_cost_inst,
                "is_feasible": bool(is_feasible),
                "solution": solution,
                "time_ms": time_ms
            })
        except Exception as e:
            error_count += 1
            results.append({"instance": instance_name, "status": "error", "error": str(e)})

    summary = {
        "category": category,
        "total_instances": len(results),
        "feasible": feasible_count,
        "errors": error_count,
        "total_time_ms": total_time,
        "avg_time_ms": total_time / len(results) if results else 0,
        "total_cost": total_cost,
        "avg_cost": total_cost / feasible_count if feasible_count > 0 else 0,
        "results": results
    }
    summary_file = os.path.join(output_dir, "summary.json")
    with open(summary_file, 'w') as f:
        json.dump(summary, f, indent=2)
    print(f"✓ {category}: {feasible_count}/{len(results)} feasible, {total_time:.2f}ms total")

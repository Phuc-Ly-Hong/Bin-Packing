#!/usr/bin/env python3
import os
import json
import glob
from datetime import datetime

all_results = {}
grand_total_instances = 0
grand_total_feasible = 0
grand_total_time = 0.0
grand_total_cost = 0

results_dirs = sorted(glob.glob("all-results/results-*"))
for results_dir in results_dirs:
    category = os.path.basename(results_dir).replace("results-", "")
    summary_file = os.path.join(results_dir, "summary.json")
    if os.path.exists(summary_file):
        try:
            with open(summary_file, 'r') as f:
                summary = json.load(f)
            all_results[category] = summary
            grand_total_instances += summary.get("total_instances", 0)
            grand_total_feasible += summary.get("feasible", 0)
            grand_total_time += summary.get("total_time_ms", 0)
            grand_total_cost += summary.get("total_cost", 0)
        except Exception as e:
            print(f"Warning: Failed to read {summary_file}: {e}")

report = "# Benchmark Report\n\n"
report += f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n"
report += "## Summary\n\n"
report += f"- **Total Instances:** {grand_total_instances}\n"
report += f"- **Feasible Solutions:** {grand_total_feasible}/{grand_total_instances}\n"
report += f"- **Total Computation Time:** {grand_total_time:.2f}ms\n"
report += f"- **Average Time per Instance:** {grand_total_time/grand_total_instances if grand_total_instances > 0 else 0:.2f}ms\n"
report += f"- **Total Cost:** {grand_total_cost}\n"
report += f"- **Average Cost:** {grand_total_cost/grand_total_feasible if grand_total_feasible > 0 else 0:.0f}\n\n"
report += "## Results by Category\n\n"
report += "| Category | Instances | Feasible | Avg Time (ms) | Avg Cost |\n"
report += "|----------|-----------|----------|---------------|----------|\n"

for category in sorted(all_results.keys()):
    summary = all_results[category]
    total = summary.get("total_instances", 0)
    feasible = summary.get("feasible", 0)
    avg_time = summary.get("avg_time_ms", 0)
    avg_cost = summary.get("avg_cost", 0)
    report += f"| {category} | {total} | {feasible} | {avg_time:.2f} | {avg_cost:.0f} |\n"

report += "\n## Detailed Results\n\n"

for category in sorted(all_results.keys()):
    summary = all_results[category]
    report += f"### {category}\n\n"
    report += f"**Summary:** {summary.get('feasible', 0)}/{summary.get('total_instances', 0)} feasible, {summary.get('total_time_ms', 0):.2f}ms\n\n"
    report += "| Instance | Cost | Feasible | Time (ms) | Solution |\n"
    report += "|----------|------|----------|-----------|----------|\n"
    for result in summary.get("results", []):
        if result.get("status") == "success":
            inst = result.get("instance", "unknown")
            cost = result.get("total_cost", 0)
            feasible = "✓" if result.get("is_feasible") else "✗"
            time_ms = result.get("time_ms", 0)
            solution = result.get("solution", "")[:80]
            if len(result.get("solution", "")) > 80:
                solution += "..."
            report += f"| {inst} | {cost} | {feasible} | {time_ms:.2f} | `{solution}` |\n"
        else:
            inst = result.get("instance", "unknown")
            error = result.get("error", "Unknown error")
            report += f"| {inst} | - | ERROR | - | {error} |\n"
    report += "\n"

with open("final-report/BENCHMARK_RESULTS.md", 'w') as f:
    f.write(report)

with open("final-report/all_results.json", 'w') as f:
    json.dump(all_results, f, indent=2)

print("✓ Aggregation complete")
print(f"  Total: {grand_total_instances} instances, {grand_total_feasible} feasible")

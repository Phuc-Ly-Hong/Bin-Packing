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
                content = f.read().strip()
            
            if not content:
                error_count += 1
                results.append({"instance": instance_name, "status": "error", "error": "Empty output"})
                continue

            # Tách nội dung thành các từ/số riêng biệt
            parts = content.split()

            # KIỂM TRA ĐỊNH DẠNG OUTPUT:
            # Nếu file chứa chuỗi tiêu đề "=========================" (Hệ thống mới)
            if "====" in content or "SOLVED" in content:
                num_packages = 0
                total_cost_inst = 0
                time_ms = 0.0
                solution_parts = []
                is_solution_block = False

                lines = content.splitlines()
                for line in lines:
                    line = line.strip()
                    if not line:
                        continue
                    if "Total Packages Served:" in line:
                        num_packages = int(line.split(":")[-1].strip())
                    elif "Total Cost:" in line:
                        # Lấy số đầu tiên sau dấu hai chấm (bỏ qua phần Penalty phía sau)
                        cost_str = line.split(":")[1].strip().split()[0]
                        total_cost_inst = int(float(cost_str))
                    elif "Execution Time:" in line:
                        # Lấy số giây và đổi sang mili-giây (ms) giống định dạng cũ của bạn
                        time_str = line.split(":")[1].strip().split()[0]
                        time_ms = float(time_str) * 1000.0
                    elif "DETAILED SOLUTION:" in line:
                        is_solution_block = True
                        continue
                    
                    # Nếu đang ở trong khối solution, lấy các cặp "id_gói id_xe"
                    if is_solution_block:
                        solution_parts.append(line)

                # Dòng đầu của solution block thường là số gói hàng (theo hàm output_solution gốc)
                # Ta gộp toàn bộ các dòng còn lại thành chuỗi solution nằm ngang cho file JSON
                solution = " ".join(solution_parts)
                is_feasible = True if total_cost_inst >= 0 else False

            # Định dạng GỐC của bạn (Chỉ gồm chuỗi số cách nhau bằng khoảng trắng trên 1 dòng)
            else:
                if len(parts) < 4:
                    error_count += 1
                    results.append({"instance": instance_name, "status": "error", "error": "Invalid output format"})
                    continue
                num_packages = int(parts[0])
                total_cost_inst = int(parts[1])
                is_feasible = bool(int(parts[2]))
                time_ms = float(parts[-1])
                solution = " ".join(parts[3:-1])

            # Tính toán tổng hợp kết quả (Chỉ cộng cost nếu giải nghiệm thành công)
            if is_feasible:
                feasible_count += 1
                total_cost += total_cost_inst
            
            total_time += time_ms

            results.append({
                "instance": instance_name,
                "status": "success",
                "num_packages": num_packages,
                "total_cost": total_cost_inst,
                "is_feasible": is_feasible,
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
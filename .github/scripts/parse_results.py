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
                lines = [line.strip() for line in f.readlines() if line.strip()]
            
            if not lines:
                error_count += 1
                results.append({"instance": instance_name, "status": "error", "error": "Empty output"})
                continue

            # Khởi tạo các biến để tìm kiếm thông tin từ file output mới
            num_packages = 0
            total_cost_inst = 0
            is_feasible = True # Hoặc sửa tùy thuộc vào điều kiện Penalty/Feasible của bạn
            time_ms = 0.0
            solution_lines = []
            is_solution_block = False

            # Duyệt từng dòng để bóc tách thông tin chính xác
            for line in lines:
                if "Total Packages Served:" in line:
                    num_packages = int(line.split(":")[-1].strip())
                elif "Total Cost:" in line:
                    # Tách chuỗi lấy phần giá trị số của cost (bỏ qua phần chữ Penalty phía sau nếu có)
                    cost_part = line.split(":")[1].strip().split()[0]
                    total_cost_inst = int(float(cost_part)) # Ép kiểu phòng trường hợp là số thực
                elif "Execution Time:" in line:
                    time_ms = float(line.split(":")[1].strip().split()[0]) * 1000.0 # Đổi s sang ms nếu cần, hoặc giữ nguyên tùy đơn vị dữ liệu cũ
                elif "DETAILED SOLUTION:" in line:
                    is_solution_block = True
                    continue
                
                # Nếu đã bước vào block Solution, gom toàn bộ các dòng id_gói id_xe lại
                if is_solution_block:
                    solution_lines.append(line)

            # Khôi phục chuỗi solution thu gọn thành 1 dòng để lưu vào json
            solution = " ".join(solution_lines)

            # Check tính đúng đắn (Mặc định nếu cost > -1 hoặc lấy được hàng thì coi như tìm được nghiệm)
            if total_cost_inst >= 0:
                feasible_count += 1
                
            total_time += time_ms
            total_cost += max(0, total_cost_inst)

            results.append({
                "instance": instance_name,
                "status": "success",
                "num_packages": num_packages,
                "total_cost": total_cost_inst,
                "is_feasible": True if total_cost_inst >= 0 else False,
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
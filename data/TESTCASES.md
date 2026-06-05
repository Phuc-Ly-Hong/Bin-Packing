# Bộ test instances — mô tả đầy đủ

Tài liệu này mô tả TOÀN BỘ bộ dữ liệu test của dự án: ý nghĩa, vai trò, vị trí, cách sinh,
định dạng file, quy ước đặt tên, và bảng liệt kê từng đề. Mục tiêu: chỉ cần đọc file này là
hiểu bộ test gồm những gì, vì sao có, và sinh lại thế nào.

> Số liệu trong các bảng dưới được trích từ `data/generated/manifest.csv` (đo lại từ chính
> các instance đã sinh). Muốn sinh lại toàn bộ: `python scripts/gen_data.py --clean`.

---

## 1. Tổng quan

Bộ test gồm **76 instance**:

- **75 đề sinh tự động** trong `data/generated/`, chia 7 nhóm (profile), do
  `scripts/gen_data.py` sinh theo thiết kế thí nghiệm có kiểm soát.
- **1 đề mẫu làm tay** `data/sample/sample_01.txt` (5 đơn, 2 xe) để demo / đọc nhanh.

Cây thư mục:

```
data/
├── TESTCASES.md              # tài liệu này
├── sample/
│   └── sample_01.txt         # 1 đề mẫu làm tay (tối ưu = 27)
└── generated/
    ├── manifest.csv          # bảng tổng máy-đọc (75 dòng + header)
    ├── exact/                # 10 — ILP giải tối ưu (mốc chân lý đo gap)
    ├── sweep_gap/            # 15 — quét γ (bề rộng cửa sổ cận dưới)
    ├── sweep_tightness/      # 15 — quét ρ (độ chật chỗ)
    ├── sweep_size/           # 10 — quét N (kích thước)
    ├── sweep_correlation/    # 10 — quét α (tương quan d↔c)
    ├── replication/          # 10 — lặp seed tại 1 điểm (đo phương sai)
    └── edge/                 # 5 — edge cases (test đúng đắn)
```

Đếm theo nhóm:

| Nhóm | Số đề | Vai trò ngắn gọn |
|---|--:|---|
| `exact_small` | 10 | Mốc chân lý — ILP giải tối ưu, để đo gap heuristic |
| `sweep_gap` | 15 | Quét γ — trục ĐẶC TRƯNG của bài toán |
| `sweep_tightness` | 15 | Quét ρ — độ chật chỗ (tới quá tải ρ>1) |
| `sweep_size` | 10 | Quét N — tách riêng ảnh hưởng kích thước |
| `sweep_correlation` | 10 | Quét α — tương quan demand↔value |
| `replication` | 10 | Lặp seed cùng một điểm — đo phương sai |
| `edge` | 5 | Edge cases — test tính đúng đắn |
| `sample` | 1 | Đề mẫu làm tay |
| **Tổng** | **76** | |

---

## 2. Nhắc lại bài toán

"Bin Packing với cận trên VÀ cận dưới sức chứa": có **N đơn hàng** và **K phương tiện**.

- Đơn `i` có demand `d[i]` (khối lượng) và value `c[i]` (giá trị).
- Xe `k` có cửa sổ sức chứa `[L_k, U_k] = [c1[k], c2[k]]`.
- Mỗi đơn được gán cho **tối đa 1 xe** (được phép bỏ trống đơn).
- Một xe **đã dùng** phải chở tổng demand nằm trong `[L_k, U_k]` (cận dưới L là điểm đặc
  trưng so với knapsack/bin-packing kinh điển).
- **Mục tiêu:** tối đa hóa tổng value của các đơn được phục vụ.

Ràng buộc kích thước & miền giá trị (mọi instance đều tôn trọng):
`N ≤ 1000`, `K ≤ 100`, `d[i], c[i] ∈ [1, 100]`, `c1[k], c2[k] ∈ [1, 1000]`, `c1[k] ≤ c2[k]`.

---

## 3. Định dạng file `.txt`

Mỗi instance là một file văn bản thuần, không comment, không dòng trống:

```
dòng 1:        N K
dòng 2..N+1:   d[i] c[i]      (mỗi đơn 1 dòng: demand rồi value)
dòng N+2..N+K+1: c1[k] c2[k]  (mỗi xe 1 dòng: cận dưới L rồi cận trên U)
```

Ví dụ `data/sample/sample_01.txt` (N=5 đơn, K=2 xe):

```
5 2          <- N=5, K=2
5 9          <- đơn 1: d=5,  c=9
7 2          <- đơn 2: d=7,  c=2
12 6         <- đơn 3: d=12, c=6
12 4         <- đơn 4: d=12, c=4
7 6          <- đơn 5: d=7,  c=6
12 14        <- xe 1: L=12, U=14
27 31        <- xe 2: L=27, U=31
```

Nghiệm tối ưu của đề mẫu này = **27** (xe 1 ← {đơn 1, đơn 5}, Σd=12∈[12,14], value 15;
xe 2 ← {đơn 2, đơn 3, đơn 4}, Σd=31∈[27,31], value 12).

Bộ đọc file dùng trong code: `read_instance()` trong `src/core/io.py` (trả về đối tượng
`Instance` của `src/core/instance.py`).

---

## 4. Metadata đi kèm

Ngoài file `.txt`, mỗi đề sinh tự động có thêm file `*.meta.json` cùng tên, và toàn bộ được
tổng hợp vào `data/generated/manifest.csv`.

Schema `*.meta.json` (ví dụ `sweep_size_004_...`):

```json
{
  "name": "...",                       // tên instance
  "seed": 5005,                        // random seed
  "requested": { "N", "K", "tightness", "gap_ratio", "correlation" },  // tham số YÊU CẦU
  "actual":    { "N", "K", "tightness", "gap_avg",  "correlation" },   // ĐO LẠI từ đề thật
  "stats":     { "total_demand", "total_capacity", "total_value",
                 "d_range", "c_range", "c1_range", "c2_range" },
  "d_max_used": 100,                   // trần demand thực dùng khi sample
  "profile": "sweep_size",
  "repeat": 0
}
```

`manifest.csv` có 16 cột: `name, profile, kind, seed, repeat, N, K, tightness, gap_avg,
correlation, req_tightness, req_gap, req_corr, total_demand, total_capacity, total_value`.

**Lưu ý quan trọng:** `requested` là tham số ta yêu cầu, `actual` là giá trị **đo lại** từ
instance đã sinh. Hai giá trị lệch nhẹ là bình thường (do làm tròn về số nguyên, clip về
biên `[1,1000]`, và lấy mẫu ngẫu nhiên hữu hạn). Các bảng dưới ghi cả hai khi cần.

---

## 5. Năm trục độ khó + điểm neo

Thiết kế bộ test xoay quanh các "trục độ khó" có thể điều khiển độc lập:

| Trục | Ký hiệu | Định nghĩa | Ý nghĩa |
|---|---|---|---|
| Kích thước | N, K | số đơn, số xe | Quy mô bài toán (N/K giữ ≈ 10) |
| Độ chật chỗ | ρ (tightness) | `Σd / Σc2` | ρ<1 dư chỗ, ρ≈1 vừa khít, ρ>1 vượt sức chứa |
| Bề rộng cửa sổ cận | γ (gap_ratio) | trung bình `(c2−c1)/c2` | γ nhỏ ⇒ cửa sổ [L,U] hẹp (gần subset-sum), γ lớn ⇒ rộng/dễ |
| Tương quan | α (correlation) | Pearson giữa d và c | α>0: đơn nặng thường đáng giá; α<0: ngược lại |

`γ` (cận dưới L) là **trục đặc trưng** phân biệt bài toán này với knapsack/bin-packing
thường: L buộc xe đã dùng phải đủ "đầy", làm bài toán khó hơn hẳn.

**Điểm neo trung tâm:** `N=80, K=8, ρ=0.85, γ=0.15, α=0`. Mỗi profile sweep cố định 4 trục ở
điểm neo và chỉ quét đúng 1 trục; các sweep đều đi qua điểm neo nên kết quả **so sánh được
với nhau**. (Định nghĩa trong `src/generators/profiles.py`: `_ANCHOR_NK`, `_ANCHOR_RHO`,
`_ANCHOR_GAP`, `_ANCHOR_CORR`.)

---

## 6. Cách sinh

Toàn bộ do `scripts/gen_data.py` sinh, dùng `src/generators/`:

```
python scripts/gen_data.py                 # sinh tất cả vào data/generated/
python scripts/gen_data.py --clean         # xóa output cũ trước khi sinh
python scripts/gen_data.py --out-dir X/Y   # đổi thư mục đích
```

Thuật toán sinh 1 instance (`generate_instance` trong `src/generators/random_gen.py`):

1. **demand `d`** — lấy mẫu đều trong `[1, d_max]`, với `d_max` **thích nghi** theo ρ và N/K
   (`target_avg_d = 700·ρ·K/N`, `d_max ≈ 2·target_avg_d`, clip về `[5,100]`) để ρ yêu cầu
   khả thi trong biên sức chứa.
2. **value `c`** — nếu α=0: đều trong `[1,100]`; nếu α>0: `c ≈ d + nhiễu` (nhiễu nhỏ ⇒ α
   cao); nếu α<0: `c ≈ (100−d) + nhiễu`.
3. **cận trên `c2`** — mỗi xe `c2 = (Σd/ρ)/K × hệ số ngẫu nhiên [0.7,1.3]`, clip `[1,1000]`
   ⇒ đạt ρ mục tiêu.
4. **cận dưới `c1`** — `c1 = c2·(1 − γ·hệ số ngẫu nhiên [0.7,1.3])`, đảm bảo `c1 ≤ c2`
   ⇒ đạt γ mục tiêu.

`seed` cố định ⇒ tái lập 100%. Lưới tham số từng profile khai báo ở
`src/generators/profiles.py`; edge cases ở `src/generators/edge_gen.py`.

---

## 7. Quy ước đặt tên

```
{profile}_{idx}_N{N}K{K}_t{ρ×100}g{γ×100}c{α×100}_seed{seed}
```

Ví dụ giải mã `sweep_gap_000_N80K8_t85g5c0_seed3001`:

| Token | Ý nghĩa |
|---|---|
| `sweep_gap` | tên profile (nhóm) |
| `000` | chỉ số thứ tự trong lưới (3 chữ số) |
| `N80K8` | N=80 đơn, K=8 xe |
| `t85` | ρ (tightness) yêu cầu × 100 = 0.85 |
| `g5` | γ (gap) yêu cầu × 100 = 0.05 |
| `c0` | α (correlation) yêu cầu × 100 = 0 (vd `c-80` ⇒ α=−0.80) |
| `seed3001` | random seed |

(Token `t/g/c` là tham số **yêu cầu** — giá trị **thực đo** xem bảng từng đề hoặc
`manifest.csv`.)

---

## 8. Mô tả từng nhóm (bảng từng đề)

### 8.1 `exact_small` — mốc chân lý (10 đề)

- **Vai trò:** đề đủ nhỏ để ILP (`ilp_ortools`) giải **tối ưu < 1s** ⇒ làm mốc chân lý đo
  gap của heuristic.
- **Lưới:** N×K ∈ {(6,2),(10,2),(14,3),(18,3),(20,4)} × ρ∈{0.75,0.90} × γ=0.20 × α=0, 1 seed.
- **Vị trí:** `data/generated/exact/` · **seed:** 1001–1010.

| # | N | K | seed | ρ yêu cầu | ρ thực | γ thực | α thực | Σd | Σc2 | Σc |
|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| 000 | 6 | 2 | 1001 | 0.75 | 0.8191 | 0.1631 | +0.610 | 308 | 376 | 255 |
| 001 | 6 | 2 | 1002 | 0.90 | 1.1381 | 0.2185 | −0.013 | 379 | 333 | 274 |
| 002 | 10 | 2 | 1003 | 0.75 | 0.9306 | 0.2352 | −0.064 | 523 | 562 | 545 |
| 003 | 10 | 2 | 1004 | 0.90 | 0.9429 | 0.1748 | +0.123 | 347 | 368 | 561 |
| 004 | 14 | 3 | 1005 | 0.75 | 0.6655 | 0.2131 | +0.026 | 762 | 1145 | 803 |
| 005 | 14 | 3 | 1006 | 0.90 | 0.9579 | 0.2389 | +0.157 | 614 | 641 | 749 |
| 006 | 18 | 3 | 1007 | 0.75 | 0.7074 | 0.2129 | +0.259 | 817 | 1155 | 966 |
| 007 | 18 | 3 | 1008 | 0.90 | 0.8510 | 0.1945 | −0.044 | 868 | 1020 | 860 |
| 008 | 20 | 4 | 1009 | 0.75 | 0.7366 | 0.2100 | +0.161 | 945 | 1283 | 991 |
| 009 | 20 | 4 | 1010 | 0.90 | 0.9019 | 0.1742 | +0.050 | 1066 | 1182 | 837 |

### 8.2 `sweep_gap` — quét γ (15 đề)

- **Vai trò:** đo ảnh hưởng của **bề rộng cửa sổ cận** γ (trục đặc trưng). γ nhỏ ⇒ gần
  subset-sum, khó hơn.
- **Lưới (cố định N80K8, ρ≈0.85, α=0):** γ∈{0.05, 0.15, 0.30, 0.50, 0.70}, mỗi mức 3 seed.
- **Vị trí:** `data/generated/sweep_gap/` · **seed:** 3001–3015.

| # | seed | γ yêu cầu | γ thực | ρ thực | α thực | Σd | Σc2 | Σc |
|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| 000 | 3001 | 0.05 | 0.0550 | 0.9413 | −0.073 | 4217 | 4480 | 4206 |
| 001 | 3002 | 0.05 | 0.0520 | 0.8676 | −0.066 | 3577 | 4123 | 3786 |
| 002 | 3003 | 0.05 | 0.0538 | 0.8382 | +0.082 | 4123 | 4919 | 4461 |
| 003 | 3004 | 0.15 | 0.1467 | 0.8381 | −0.079 | 4461 | 5323 | 4345 |
| 004 | 3005 | 0.15 | 0.1545 | 0.9078 | −0.044 | 4373 | 4817 | 3841 |
| 005 | 3006 | 0.15 | 0.1500 | 0.8159 | −0.098 | 4174 | 5116 | 4158 |
| 006 | 3007 | 0.30 | 0.3026 | 0.8866 | +0.018 | 3512 | 3961 | 4067 |
| 007 | 3008 | 0.30 | 0.2996 | 0.8524 | +0.149 | 4054 | 4756 | 3623 |
| 008 | 3009 | 0.30 | 0.3290 | 0.9010 | −0.143 | 4176 | 4635 | 4639 |
| 009 | 3010 | 0.50 | 0.5461 | 0.8599 | +0.152 | 4541 | 5281 | 4383 |
| 010 | 3011 | 0.50 | 0.4523 | 0.7931 | +0.014 | 3914 | 4935 | 4168 |
| 011 | 3012 | 0.50 | 0.4913 | 0.9455 | +0.074 | 4167 | 4407 | 3993 |
| 012 | 3013 | 0.70 | 0.6864 | 0.8705 | −0.135 | 3826 | 4395 | 3545 |
| 013 | 3014 | 0.70 | 0.7127 | 0.8027 | +0.100 | 3722 | 4637 | 4015 |
| 014 | 3015 | 0.70 | 0.6581 | 0.8633 | −0.049 | 3845 | 4454 | 4007 |

### 8.3 `sweep_tightness` — quét ρ (15 đề)

- **Vai trò:** đo ảnh hưởng của **độ chật chỗ** ρ, từ dư chỗ tới **vượt sức chứa (ρ>1)**.
- **Lưới (cố định N80K8, γ≈0.15, α=0):** ρ∈{0.60, 0.75, 0.85, 0.95, 1.05}, mỗi mức 3 seed.
- **Vị trí:** `data/generated/sweep_tightness/` · **seed:** 4001–4015.

| # | seed | ρ yêu cầu | ρ thực | γ thực | α thực | Σd | Σc2 | Σc |
|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| 000 | 4001 | 0.60 | 0.6043 | 0.1616 | −0.034 | 3297 | 5456 | 3981 |
| 001 | 4002 | 0.60 | 0.6242 | 0.1330 | −0.027 | 3418 | 5476 | 4375 |
| 002 | 4003 | 0.60 | 0.6301 | 0.1379 | +0.026 | 3201 | 5080 | 3814 |
| 003 | 4004 | 0.75 | 0.7257 | 0.1580 | +0.010 | 4197 | 5783 | 4330 |
| 004 | 4005 | 0.75 | 0.7782 | 0.1645 | +0.117 | 4123 | 5298 | 3639 |
| 005 | 4006 | 0.75 | 0.7668 | 0.1440 | +0.079 | 3870 | 5047 | 4262 |
| 006 | 4007 | 0.85 | 0.9367 | 0.1514 | −0.105 | 3821 | 4079 | 3978 |
| 007 | 4008 | 0.85 | 0.7953 | 0.1601 | −0.042 | 3838 | 4826 | 4011 |
| 008 | 4009 | 0.85 | 0.7604 | 0.1353 | −0.098 | 4173 | 5488 | 4619 |
| 009 | 4010 | 0.95 | 0.9442 | 0.1469 | −0.038 | 4215 | 4464 | 3933 |
| 010 | 4011 | 0.95 | 1.0480 | 0.1499 | +0.172 | 4345 | 4146 | 4060 |
| 011 | 4012 | 0.95 | 1.0412 | 0.1348 | +0.029 | 4369 | 4196 | 4409 |
| 012 | 4013 | 1.05 | 0.9892 | 0.1376 | +0.003 | 4233 | 4279 | 3883 |
| 013 | 4014 | 1.05 | 1.2414 | 0.1380 | +0.100 | 3878 | 3124 | 3469 |
| 014 | 4015 | 1.05 | 1.0624 | 0.1591 | −0.009 | 3964 | 3731 | 3843 |

Khi ρ>1 (tổng demand vượt tổng sức chứa) thì **không thể phục vụ hết** ⇒ buộc bỏ bớt đơn,
đây là vùng heuristic phải khéo chọn.

### 8.4 `sweep_size` — quét N (10 đề)

- **Vai trò:** tách riêng ảnh hưởng **kích thước** (giữ ρ, γ, α ở điểm neo; N/K=10). Đây là
  nhóm cho thấy CP-SAT sụp đổ theo N (xem §2.6 `ALGORITHM_DIRECTIONS.md`).
- **Lưới (cố định ρ≈0.85, γ≈0.15, α=0):** N∈{30,100,300,600,1000}, K=N/10, mỗi mức 2 seed.
- **Vị trí:** `data/generated/sweep_size/` · **seed:** 5001–5010.

| # | seed | N | K | ρ thực | γ thực | α thực | Σd | Σc2 | Σc |
|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| 000 | 5001 | 30 | 3 | 0.7686 | 0.1713 | +0.414 | 1531 | 1992 | 1661 |
| 001 | 5002 | 30 | 3 | 0.8036 | 0.1434 | −0.310 | 1604 | 1996 | 1521 |
| 002 | 5003 | 100 | 10 | 0.8448 | 0.1284 | −0.144 | 4824 | 5710 | 5436 |
| 003 | 5004 | 100 | 10 | 0.8910 | 0.1339 | −0.036 | 4579 | 5139 | 4709 |
| 004 | 5005 | 300 | 30 | 0.8405 | 0.1509 | −0.012 | 15681 | 18656 | 15216 |
| 005 | 5006 | 300 | 30 | 0.8504 | 0.1569 | −0.062 | 14960 | 17592 | 15075 |
| 006 | 5007 | 600 | 60 | 0.8652 | 0.1515 | −0.020 | 30209 | 34915 | 30327 |
| 007 | 5008 | 600 | 60 | 0.8183 | 0.1526 | −0.006 | 30943 | 37816 | 29758 |
| 008 | 5009 | 1000 | 100 | 0.8584 | 0.1480 | +0.048 | 50584 | 58931 | 51201 |
| 009 | 5010 | 1000 | 100 | 0.8498 | 0.1481 | +0.025 | 50733 | 59703 | 49887 |

### 8.5 `sweep_correlation` — quét α (10 đề)

- **Vai trò:** đo ảnh hưởng của **tương quan demand↔value** α (từ âm tới dương).
- **Lưới (cố định N100K10, ρ≈0.85, γ≈0.15):** α∈{−0.8,−0.4,0,0.4,0.8}, mỗi mức 2 seed.
- **Vị trí:** `data/generated/sweep_correlation/` · **seed:** 6001–6010.

| # | seed | α yêu cầu | α thực | ρ thực | γ thực | Σd | Σc2 | Σc |
|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| 000 | 6001 | −0.8 | −0.985 | 0.8599 | 0.1384 | 5204 | 6052 | 4888 |
| 001 | 6002 | −0.8 | −0.978 | 0.8757 | 0.1529 | 5022 | 5735 | 5089 |
| 002 | 6003 | −0.4 | −0.863 | 0.8115 | 0.1506 | 4803 | 5919 | 5225 |
| 003 | 6004 | −0.4 | −0.869 | 0.8430 | 0.1419 | 5213 | 6184 | 5204 |
| 004 | 6005 | 0.0 | +0.067 | 0.7931 | 0.1523 | 5125 | 6462 | 5019 |
| 005 | 6006 | 0.0 | +0.170 | 0.8832 | 0.1357 | 4740 | 5367 | 4971 |
| 006 | 6007 | 0.4 | +0.796 | 0.8933 | 0.1569 | 5107 | 5717 | 5178 |
| 007 | 6008 | 0.4 | +0.879 | 0.8747 | 0.1476 | 5097 | 5827 | 5459 |
| 008 | 6009 | 0.8 | +0.976 | 0.8473 | 0.1426 | 5539 | 6537 | 5542 |
| 009 | 6010 | 0.8 | +0.984 | 0.9279 | 0.1505 | 4930 | 5313 | 4865 |

Lưu ý: α **thực** thường đậm hơn α yêu cầu (vd yêu cầu ±0.4 nhưng đo được ≈ ±0.87) — do mô
hình nhiễu tạo tương quan mạnh. Token tên file (`c40`, `c-80`...) là giá trị **yêu cầu**.

### 8.6 `replication` — lặp seed (10 đề)

- **Vai trò:** cùng một điểm tham số (điểm neo mở rộng N100K10), đổi seed 10 lần ⇒ đo
  **phương sai** giữa các đề (quan trọng để đánh giá metaheuristic ngẫu nhiên).
- **Lưới (cố định N100K10, ρ≈0.85, γ≈0.15, α=0):** 10 seed.
- **Vị trí:** `data/generated/replication/` · **seed:** 7001–7010.

| # | seed | ρ thực | γ thực | α thực | Σd | Σc2 | Σc |
|--:|--:|--:|--:|--:|--:|--:|--:|
| 000 | 7001 | 0.8708 | 0.1541 | +0.007 | 5168 | 5935 | 5304 |
| 001 | 7002 | 0.8225 | 0.1514 | +0.132 | 4846 | 5892 | 4917 |
| 002 | 7003 | 0.8849 | 0.1608 | −0.126 | 4837 | 5466 | 5302 |
| 003 | 7004 | 0.8568 | 0.1551 | +0.122 | 4744 | 5537 | 5184 |
| 004 | 7005 | 0.9078 | 0.1557 | −0.082 | 4993 | 5500 | 4667 |
| 005 | 7006 | 0.7927 | 0.1558 | +0.236 | 5004 | 6313 | 5315 |
| 006 | 7007 | 0.8826 | 0.1607 | +0.011 | 5066 | 5740 | 5055 |
| 007 | 7008 | 0.7955 | 0.1429 | +0.055 | 5289 | 6649 | 4746 |
| 008 | 7009 | 0.8455 | 0.1599 | −0.043 | 5276 | 6240 | 4324 |
| 009 | 7010 | 0.9006 | 0.1536 | −0.101 | 5683 | 6310 | 5126 |

### 8.7 `sample` — đề mẫu làm tay (1 đề)

- **Vai trò:** đề cực nhỏ (N=5, K=2) viết tay để demo định dạng, kiểm thử nhanh, ví dụ trong
  tài liệu. Nghiệm tối ưu = 27.
- **Vị trí:** `data/sample/sample_01.txt` (KHÔNG nằm trong `manifest.csv`).

---

## 9. Edge cases + vai trò trong dự án

### 9.1 Edge cases (5 đề, `data/generated/edge/`)

Mục đích: **stress-test tính đúng đắn** của thuật toán ở các trường hợp biên.

| Tên | loại | N | K | seed | Σd | Σc2 | Σc | Đặc điểm / kỳ vọng |
|---|---|--:|--:|--:|--:|--:|--:|---|
| `edge_single_vehicle_seed9001` | single_vehicle | 15 | 1 | 9001 | 437 | 305 | 731 | K=1 ⇒ quy về Knapsack đơn (sức chứa ≈ 70% Σd ⇒ buộc chọn subset) |
| `edge_single_vehicle_seed9011` | single_vehicle | 8 | 1 | 9011 | 201 | 140 | 551 | K=1, biến thể nhỏ hơn |
| `edge_single_order_seed9002` | single_order | 1 | 5 | 9002 | 32 | 400 | 94 | N=1 trivial; nửa số xe chứa được đơn, nửa không (c1 > d) |
| `edge_no_lower_seed9003` | no_lower | 30 | 5 | 9003 | 1399 | 1556 | 1542 | L_k=1 ∀k ⇒ cận dưới mất hiệu lực (quy về bài toán chỉ có cận trên) |
| `edge_infeasible_seed9004` | infeasible | 30 | 5 | 9004 | 164 | 1850 | 1466 | L_k > Σd ⇒ không xe nào dùng được ⇒ **nghiệm tối ưu = 0** (rỗng) |

(Định nghĩa generator: `gen_single_vehicle`, `gen_single_order`, `gen_no_lower_bound`,
`gen_infeasible_high_lower` trong `src/generators/edge_gen.py`.)

### 9.2 Nhóm nào dùng ở đâu

| Nhóm | Dùng cho | Liên kết |
|---|---|---|
| `exact_small` | Mốc chân lý đo gap heuristic | `tests/`, `experiments/explore_directions.py` |
| `sweep_gap` / `sweep_tightness` / `sweep_size` / `sweep_correlation` | Tách bạch từng trục độ khó; chứng minh CP-SAT sụp đổ theo N | §2.6 / §2.7 `ALGORITHM_DIRECTIONS.md`, `experiments/explore_directions.py`, `experiments/bench_h0.py` |
| `replication` | Đo phương sai giữa các đề (đánh giá metaheuristic) | dành cho thí nghiệm metaheuristic |
| `edge` | Test tính đúng đắn (K=1, N=1, no-lower, infeasible) | `tests/` |
| `sample` | Demo / smoke test nhanh | tài liệu, kiểm thử thủ công |

Bảng kết quả thực nghiệm trên bộ test này nằm ở `results/tables/` (vd
`explore_directions.csv`, `bench_h0.csv`, `verify_small.csv`).

---

*Sinh lại toàn bộ bộ test:* `python scripts/gen_data.py --clean`.
*Tài liệu này mô tả ảnh chụp hiện tại của `data/`; nếu đổi lưới tham số trong
`src/generators/profiles.py` rồi sinh lại, hãy cập nhật lại các bảng ở đây.*

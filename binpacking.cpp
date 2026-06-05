#include <bits/stdc++.h>
#include <unordered_set>
#include <chrono>
using namespace std;

// ==================== DATA STRUCTURES ====================

struct Order {
    int id;  // 1-indexed
    int d;   // quantity
    int c;   // cost/value
};

struct Vehicle {
    int id;  // 1-indexed
    int c1;  // lower bound
    int c2;  // upper bound
};

struct Solution {
    // bins[0] = unassigned, bins[1..K] = vehicle assignments
    vector<vector<int>> bins;
    vector<int> bin_load; // Tải trọng hiện tại của từng xe (O(1) delta)
    double total_cost;
    double penalty;
    double fitness;    // = -total_cost + alpha * penalty
    bool   is_feasible;

    Solution() : total_cost(0), penalty(0), fitness(DBL_MAX), is_feasible(false) {}
};

struct TabuMove {
    string type;                        // "1-0","1-1","2-0","2-1","2-2"
    int order1, order2, order3, order4; // order IDs involved (-1 if unused)
    int v1, v2;                         // from vehicle, to vehicle (0 = unassigned)
    int tenure;                         // remaining tabu iterations
};

// ==================== GLOBALS ====================

int N, K;
vector<Order>   orders;    // 0-indexed (orders[i] has id = i+1)
vector<Vehicle> vehicles;  // 0-indexed (vehicles[k] has id = k+1)

double alpha          = 100.0;
const double Beta     = 0.3;
const double EPSILON  = 1e-9;

int MAX_ITER;
int TABU_TENURE;
int MAX_NO_IMPROVE;
int SEGMENT_LENGTH;

vector<string> MOVE_SET = {"1-0", "1-1", "2-0", "2-1", "2-2"};
vector<double> weights  = {1.0,   1.0,   1.0,   1.0,   1.0};
vector<double> scorePi  = {0.0,   0.0,   0.0,   0.0,   0.0};
vector<double> used_cnt = {0.0,   0.0,   0.0,   0.0,   0.0};

const double delta1 = 0.3;
const double delta2 = 0.2;
const double delta3 = 0.1;
const double delta4 = 0.3;

// ==================== MOVE SELECTION ====================

int select_move_type() {
    double total = accumulate(weights.begin(), weights.end(), 0.0);
    double r = ((double)rand() / RAND_MAX) * total;
    double cum = 0.0;
    for (int i = 0; i < (int)weights.size(); i++) {
        cum += weights[i];
        if (r <= cum) return i;
    }
    return (int)weights.size() - 1;
}

void update_weights() {
    for (int i = 0; i < (int)MOVE_SET.size(); i++) {
        if (used_cnt[i] > 0) {
            double avg = scorePi[i] / used_cnt[i];
            weights[i] = (1.0 - delta4) * weights[i] + delta4 * avg;
            if (weights[i] < 0.01) weights[i] = 0.01;
        }
        scorePi[i] = 0.0;
        used_cnt[i] = 0.0;
    }
}

// ==================== INPUT / OUTPUT ====================

void read_input(istream& in) {
    if (!(in >> N >> K)) return;

    orders.resize(N);
    for (int i = 0; i < N; i++) {
        orders[i].id = i + 1;
        in >> orders[i].d >> orders[i].c;
    }

    vehicles.resize(K);
    for (int k = 0; k < K; k++) {
        vehicles[k].id = k + 1;
        in >> vehicles[k].c1 >> vehicles[k].c2;
    }

    // Tối ưu cho N lớn: Giảm iterations nhưng tăng tốc độ
    if (N >= 500) {
        MAX_ITER = 1500;       SEGMENT_LENGTH = 150; MAX_NO_IMPROVE = 300;
    } else if (N >= 100) {
        MAX_ITER = 1000;       SEGMENT_LENGTH = 100; MAX_NO_IMPROVE = 200;
    } else { // N < 100
        MAX_ITER = 800;        SEGMENT_LENGTH = 60;  MAX_NO_IMPROVE = 150;
    }
    TABU_TENURE = 5; 
}

void output_solution(const Solution& sol, double elapsed_ms = 0.0) {
    vector<pair<int,int>> served;
    for (int k = 1; k <= K; k++) {
        for (int oid : sol.bins[k]) {
            served.push_back({oid, k});
        }
    }
    sort(served.begin(), served.end());

    // Format: num_packages totalCost isFeisible solution_string time_ms
    cout << served.size() << " " << (int)sol.total_cost << " " 
         << (sol.is_feasible ? "1" : "0");
    
    for (auto& p : served) {
        cout << " " << p.first << " " << p.second;
    }
    
    cout << " " << fixed << setprecision(2) << elapsed_ms << "\n";
}

// ==================== EVALUATION ====================

inline double get_perf_penalty(int k, int load) {
    if (k == 0) return 0.0;
    if (load == 0) return 0.0;   
    if (load < vehicles[k-1].c1) {
        return (vehicles[k-1].c1 - load);
    } else if (load > vehicles[k-1].c2) {
        return (load - vehicles[k-1].c2);
    }
    return 0.0;
}

void evaluate_solution(Solution& sol) {
    sol.total_cost  = 0;
    sol.penalty     = 0;
    sol.is_feasible = true;
    sol.bin_load.assign(K + 1, 0);

    for (int k = 0; k <= K; k++) {
        for (int oid : sol.bins[k]) {
            sol.bin_load[k] += orders[oid - 1].d;
            if (k > 0) {
                sol.total_cost += orders[oid - 1].c;
            }
        }
    }

    for (int k = 1; k <= K; k++) {
        double p = get_perf_penalty(k, sol.bin_load[k]);
        if (p > 0) {
            sol.penalty += p;
            sol.is_feasible = false;
        }
    }

    sol.fitness = -sol.total_cost + alpha * sol.penalty;
}

// ==================== INITIAL SOLUTION (Greedy) ====================

Solution init_greedy() {
    Solution sol;
    sol.bins.resize(K + 1);

    vector<int> ids(N);
    iota(ids.begin(), ids.end(), 1);
    sort(ids.begin(), ids.end(), [](int a, int b) {
        return orders[a-1].c > orders[b-1].c;
    });

    vector<int> qty(K + 1, 0);

    for (int oid : ids) {
        int d = orders[oid - 1].d;
        int best_k   = 0;
        int best_rem = INT_MAX;

        for (int k = 1; k <= K; k++) {
            if (qty[k] + d <= vehicles[k-1].c2) {
                int rem = vehicles[k-1].c2 - (qty[k] + d);
                if (rem < best_rem) {
                    best_rem = rem;
                    best_k   = k;
                }
            }
        }

        sol.bins[best_k].push_back(oid);
        if (best_k > 0) qty[best_k] += d;
    }

    evaluate_solution(sol);
    return sol;
}

// ==================== TABU CHECK ====================

bool is_tabu(const vector<TabuMove>& tl, const TabuMove& m) {
    auto same_pair = [](int a1, int a2, int b1, int b2) {
        return (a1 == b1 && a2 == b2) || (a1 == b2 && a2 == b1);
    };

    for (const auto& t : tl) {
        if (t.type != m.type || t.tenure <= 0) continue;

        if (m.type == "1-0") {
            if (t.order1 == m.order1 && t.v1 == m.v1 && t.v2 == m.v2)
                return true;
        }
        else if (m.type == "1-1") {
            if (same_pair(t.order1, t.order3, m.order1, m.order3) &&
                ((t.v1 == m.v1 && t.v2 == m.v2) || (t.v1 == m.v2 && t.v2 == m.v1)))
                return true;
        }
        else if (m.type == "2-0") {
            if (same_pair(t.order1, t.order2, m.order1, m.order2) &&
                t.v1 == m.v1 && t.v2 == m.v2)
                return true;
        }
        else if (m.type == "2-1") {
            if (same_pair(t.order1, t.order2, m.order1, m.order2) &&
                t.order3 == m.order3 && t.v1 == m.v1 && t.v2 == m.v2)
                return true;
            if (t.order1 == m.order3 && t.v1 == m.v2 && t.v2 == m.v1)
                return true;
        }
        else if (m.type == "2-2") {
            if (same_pair(t.order1, t.order2, m.order1, m.order2) &&
                same_pair(t.order3, t.order4, m.order3, m.order4) &&
                t.v1 == m.v1 && t.v2 == m.v2)
                return true;
            if (same_pair(t.order1, t.order2, m.order3, m.order4) &&
                same_pair(t.order3, t.order4, m.order1, m.order2) &&
                t.v1 == m.v2 && t.v2 == m.v1)
                return true;
        }
    }
    return false;
}

// ==================== O(1) DELTA UPDATE OPERATORS ====================
// Thay vì dùng hàm erase tốn O(N), ta tráo phần tử cần xóa xuống cuối vector rồi pop_back() trong O(1)

void apply_1_0(Solution& sol, int v1, int pos1, int v2, double new_cost, double new_penalty, double new_fitness) {
    int oid = sol.bins[v1][pos1];
    int d = orders[oid - 1].d;

    // Erase phần tử tại pos1 mất O(1) bằng cách swap với phần tử cuối cùng
    swap(sol.bins[v1][pos1], sol.bins[v1].back());
    sol.bins[v1].pop_back();

    // Push vào xe mới
    sol.bins[v2].push_back(oid);

    // Cập nhật tải trọng xe
    sol.bin_load[v1] -= d;
    sol.bin_load[v2] += d;

    // Gán kết quả Delta thu được từ vòng duyệt lân cận
    sol.total_cost = new_cost;
    sol.penalty = new_penalty;
    sol.fitness = new_fitness;
    sol.is_feasible = (new_penalty < EPSILON);
}

void apply_1_1(Solution& sol, int v1, int pos1, int v2, int pos2, double new_cost, double new_penalty, double new_fitness) {
    int oid1 = sol.bins[v1][pos1];
    int oid2 = sol.bins[v2][pos2];

    swap(sol.bins[v1][pos1], sol.bins[v2][pos2]);

    if (v1 != v2) {
        sol.bin_load[v1] += orders[oid2 - 1].d - orders[oid1 - 1].d;
        sol.bin_load[v2] += orders[oid1 - 1].d - orders[oid2 - 1].d;
    }

    sol.total_cost = new_cost;
    sol.penalty = new_penalty;
    sol.fitness = new_fitness;
    sol.is_feasible = (new_penalty < EPSILON);
}

void apply_2_0(Solution& sol, int v1, int pos1a, int pos1b, int v2, double new_cost, double new_penalty, double new_fitness) {
    int oid1 = sol.bins[v1][pos1a];
    int oid2 = sol.bins[v1][pos1b];
    int d_total = orders[oid1 - 1].d + orders[oid2 - 1].d;

    int hi = max(pos1a, pos1b);
    int lo = min(pos1a, pos1b);

    // Xóa hi trước để tránh lệch chỉ số lo
    swap(sol.bins[v1][hi], sol.bins[v1].back());
    sol.bins[v1].pop_back();
    swap(sol.bins[v1][lo], sol.bins[v1].back());
    sol.bins[v1].pop_back();

    sol.bins[v2].push_back(oid1);
    sol.bins[v2].push_back(oid2);

    sol.bin_load[v1] -= d_total;
    sol.bin_load[v2] += d_total;

    sol.total_cost = new_cost;
    sol.penalty = new_penalty;
    sol.fitness = new_fitness;
    sol.is_feasible = (new_penalty < EPSILON);
}

void apply_2_1(Solution& sol, int v1, int pos1a, int pos1b, int v2, int pos2, double new_cost, double new_penalty, double new_fitness) {
    int oid1 = sol.bins[v1][pos1a];
    int oid2 = sol.bins[v1][pos1b];
    int oid3 = sol.bins[v2][pos2];

    int d12 = orders[oid1 - 1].d + orders[oid2 - 1].d;
    int d3 = orders[oid3 - 1].d;

    // Xóa v1
    int hi = max(pos1a, pos1b);
    int lo = min(pos1a, pos1b);
    swap(sol.bins[v1][hi], sol.bins[v1].back());
    sol.bins[v1].pop_back();
    swap(sol.bins[v1][lo], sol.bins[v1].back());
    sol.bins[v1].pop_back();

    // Xóa v2
    swap(sol.bins[v2][pos2], sol.bins[v2].back());
    sol.bins[v2].pop_back();

    // Thêm chéo chèn ngược lại
    sol.bins[v1].push_back(oid3);
    sol.bins[v2].push_back(oid1);
    sol.bins[v2].push_back(oid2);

    sol.bin_load[v1] += d3 - d12;
    sol.bin_load[v2] += d12 - d3;

    sol.total_cost = new_cost;
    sol.penalty = new_penalty;
    sol.fitness = new_fitness;
    sol.is_feasible = (new_penalty < EPSILON);
}

void apply_2_2(Solution& sol, int v1, int pos1a, int pos1b, int v2, int pos2a, int pos2b, double new_cost, double new_penalty, double new_fitness) {
    int oid1 = sol.bins[v1][pos1a];
    int oid2 = sol.bins[v1][pos1b];
    int oid3 = sol.bins[v2][pos2a];
    int oid4 = sol.bins[v2][pos2b];

    int d12 = orders[oid1 - 1].d + orders[oid2 - 1].d;
    int d34 = orders[oid3 - 1].d + orders[oid4 - 1].d;

    // Xóa v1
    int hi1 = max(pos1a, pos1b), lo1 = min(pos1a, pos1b);
    swap(sol.bins[v1][hi1], sol.bins[v1].back());
    sol.bins[v1].pop_back();
    swap(sol.bins[v1][lo1], sol.bins[v1].back());
    sol.bins[v1].pop_back();

    // Xóa v2
    int hi2 = max(pos2a, pos2b), lo2 = min(pos2a, pos2b);
    swap(sol.bins[v2][hi2], sol.bins[v2].back());
    sol.bins[v2].pop_back();
    swap(sol.bins[v2][lo2], sol.bins[v2].back());
    sol.bins[v2].pop_back();

    sol.bins[v1].push_back(oid3);
    sol.bins[v1].push_back(oid4);
    sol.bins[v2].push_back(oid1);
    sol.bins[v2].push_back(oid2);

    sol.bin_load[v1] += d34 - d12;
    sol.bin_load[v2] += d12 - d34;

    sol.total_cost = new_cost;
    sol.penalty = new_penalty;
    sol.fitness = new_fitness;
    sol.is_feasible = (new_penalty < EPSILON);
}

// ==================== TABU SEARCH ====================

Solution tabu_search() {
    Solution current = init_greedy();
    Solution best    = current;

    vector<TabuMove> tabu_list;
    int no_improve = 0;
    int seg_count  = 0;

    const int MAX_NEIGHBORS = 15000; 
    vector<int> V(K + 1);
    iota(V.begin(), V.end(), 0);

    for (int iter = 0; iter < MAX_ITER && no_improve < MAX_NO_IMPROVE; iter++) {

        for (int i = K; i > 0; i--) {
            int j = rand() % (i + 1);
            swap(V[i], V[j]);
        }

        int     mt_idx = select_move_type();
        string mt     = MOVE_SET[mt_idx];
        used_cnt[mt_idx]++;

        // Adaptive MAX_NEIGHBORS based on instance size
        int MAX_NEIGHBORS = (N >= 500) ? 3000 : (N >= 100) ? 5000 : 8000;
        
        double   best_nf = DBL_MAX;
        TabuMove best_tm = {"", -1, -1, -1, -1, -1, -1, 0};
        
        int best_v1 = -1, best_v2 = -1;
        int best_p1a = -1, best_p1b = -1, best_p2a = -1, best_p2b = -1;
        double final_cost = 0, final_penalty = 0;
        
        bool improved = false;
        bool found    = false;
        int neighbor_cnt = 0;

        auto try_move = [&](double ns_fitness, double ns_cost, double ns_penalty, bool ns_is_feasible, 
                            const TabuMove& tm, int v1, int v2, int p1a, int p1b, int p2a, int p2b) {
            bool tabu = is_tabu(tabu_list, tm);
            if (ns_is_feasible && ns_fitness < best.fitness - EPSILON) {
                best_nf  = ns_fitness; final_cost = ns_cost; final_penalty = ns_penalty;
                best_tm  = tm;
                best_v1 = v1; best_v2 = v2;
                best_p1a = p1a; best_p1b = p1b; best_p2a = p2a; best_p2b = p2b;
                improved = true; found    = true;
            } else if (!improved && !tabu && ns_fitness < best_nf - EPSILON) {
                best_nf  = ns_fitness; final_cost = ns_cost; final_penalty = ns_penalty;
                best_tm  = tm;
                best_v1 = v1; best_v2 = v2;
                best_p1a = p1a; best_p1b = p1b; best_p2a = p2a; best_p2b = p2b;
                found    = true;
            }
        };

        // ==================== MOVE 1-0 ====================
        if (mt == "1-0") {
            int v_samples = min(20, (int)V.size());
            for (int sv = 0; sv < v_samples && neighbor_cnt <= MAX_NEIGHBORS; sv++) {
                int v1 = V[rand() % V.size()];
                
                int order_samples = min(50, (int)current.bins[v1].size());
                for (int so = 0; so < order_samples && neighbor_cnt <= MAX_NEIGHBORS; so++) {
                    int p1 = rand() % current.bins[v1].size();
                    int oid = current.bins[v1][p1];
                    int d = orders[oid - 1].d;
                    int c = orders[oid - 1].c;
                    
                    for (int v2 : V) {
                        if (v1 == v2) continue;
                        if (++neighbor_cnt > MAX_NEIGHBORS) break;

                        double new_total_cost = current.total_cost;
                        if (v1 == 0 && v2 > 0) new_total_cost += c;
                        else if (v1 > 0 && v2 == 0) new_total_cost -= c;

                        double new_penalty = current.penalty 
                            - get_perf_penalty(v1, current.bin_load[v1]) 
                            - get_perf_penalty(v2, current.bin_load[v2])
                            + get_perf_penalty(v1, current.bin_load[v1] - d) 
                            + get_perf_penalty(v2, current.bin_load[v2] + d);

                        double new_fitness = -new_total_cost + alpha * new_penalty;
                        try_move(new_fitness, new_total_cost, new_penalty, (new_penalty < EPSILON),
                                 {"1-0", oid, -1, -1, -1, v1, v2, TABU_TENURE}, v1, v2, p1, -1, -1, -1);
                    }
                    if (neighbor_cnt > MAX_NEIGHBORS) break;
                }
                if (neighbor_cnt > MAX_NEIGHBORS) break;
            }
        }

        // ==================== MOVE 1-1 ====================
        else if (mt == "1-1") {
            int v_samples = min(20, (int)V.size());
            for (int sv1 = 0; sv1 < v_samples && neighbor_cnt <= MAX_NEIGHBORS; sv1++) {
                int v1 = V[rand() % V.size()];
                
                int order_samples_v1 = min(40, (int)current.bins[v1].size());
                for (int so1 = 0; so1 < order_samples_v1 && neighbor_cnt <= MAX_NEIGHBORS; so1++) {
                    int p1 = rand() % current.bins[v1].size();
                    int oid1 = current.bins[v1][p1];
                    int d1 = orders[oid1 - 1].d;
                    int c1 = orders[oid1 - 1].c;
                    
                    for (int sv2 = 0; sv2 < v_samples && neighbor_cnt <= MAX_NEIGHBORS; sv2++) {
                        int v2 = V[rand() % V.size()];
                        if (current.bins[v2].empty()) continue;
                        
                        int order_samples_v2 = min(40, (int)current.bins[v2].size());
                        for (int so2 = 0; so2 < order_samples_v2 && neighbor_cnt <= MAX_NEIGHBORS; so2++) {
                            int p2 = rand() % current.bins[v2].size();
                            if (v1 == v2 && p1 == p2) continue;
                            if (++neighbor_cnt > MAX_NEIGHBORS) break;
                            
                            int oid2 = current.bins[v2][p2];
                            int d2 = orders[oid2 - 1].d;
                            int c2 = orders[oid2 - 1].c;

                            double new_total_cost = current.total_cost;
                            double new_penalty = current.penalty;
                            if (v1 != v2) {
                                if (v1 == 0 && v2 > 0) new_total_cost += (c1 - c2);
                                else if (v1 > 0 && v2 == 0) new_total_cost += (c2 - c1);

                                new_penalty = current.penalty 
                                    - get_perf_penalty(v1, current.bin_load[v1]) 
                                    - get_perf_penalty(v2, current.bin_load[v2])
                                    + get_perf_penalty(v1, current.bin_load[v1] - d1 + d2) 
                                    + get_perf_penalty(v2, current.bin_load[v2] - d2 + d1);
                            }

                            double new_fitness = -new_total_cost + alpha * new_penalty;
                            try_move(new_fitness, new_total_cost, new_penalty, (new_penalty < EPSILON),
                                     {"1-1", oid1, -1, oid2, -1, v1, v2, TABU_TENURE}, v1, v2, p1, -1, p2, -1);
                        }
                    }
                }
            }
        }

        // ==================== MOVE 2-0 ====================
        else if (mt == "2-0") {
            int samples = min(20, (int)V.size()); // Limit vehicles sampled
            for (int s_v1 = 0; s_v1 < samples && neighbor_cnt <= MAX_NEIGHBORS; s_v1++) {
                int v1 = V[rand() % V.size()];
                if ((int)current.bins[v1].size() < 2) continue;
                
                // Sample pairs within v1
                int sample_pairs = min(50, max(1, (int)current.bins[v1].size() * (int)current.bins[v1].size() / 10));
                for (int sp = 0; sp < sample_pairs && neighbor_cnt <= MAX_NEIGHBORS; sp++) {
                    int p1 = rand() % current.bins[v1].size();
                    int p2 = rand() % current.bins[v1].size();
                    if (p1 == p2) continue;
                    if (p1 > p2) swap(p1, p2);
                    
                    int oid1 = current.bins[v1][p1];
                    int d1 = orders[oid1 - 1].d;
                    int c1 = orders[oid1 - 1].c;
                    int oid2 = current.bins[v1][p2];
                    int d2 = orders[oid2 - 1].d;
                    int c2 = orders[oid2 - 1].c;
                    
                    for (int s_v2 = 0; s_v2 < samples && neighbor_cnt <= MAX_NEIGHBORS; s_v2++) {
                        int v2 = V[rand() % V.size()];
                        if (v1 == v2) continue;
                        if (++neighbor_cnt > MAX_NEIGHBORS) break;

                        double new_total_cost = current.total_cost;
                        if (v1 == 0 && v2 > 0) new_total_cost += (c1 + c2);
                        else if (v1 > 0 && v2 == 0) new_total_cost -= (c1 + c2);

                        double new_penalty = current.penalty 
                            - get_perf_penalty(v1, current.bin_load[v1]) 
                            - get_perf_penalty(v2, current.bin_load[v2])
                            + get_perf_penalty(v1, current.bin_load[v1] - d1 - d2) 
                            + get_perf_penalty(v2, current.bin_load[v2] + d1 + d2);

                        double new_fitness = -new_total_cost + alpha * new_penalty;
                        try_move(new_fitness, new_total_cost, new_penalty, (new_penalty < EPSILON),
                                 {"2-0", oid1, oid2, -1, -1, v1, v2, TABU_TENURE}, v1, v2, p1, p2, -1, -1);
                    }
                }
            }
        }

        // ==================== MOVE 2-1 ====================
        else if (mt == "2-1") {
            int samples = min(15, (int)V.size());
            for (int s_v1 = 0; s_v1 < samples && neighbor_cnt <= MAX_NEIGHBORS; s_v1++) {
                int v1 = V[rand() % V.size()];
                if ((int)current.bins[v1].size() < 2) continue;
                
                int sample_pairs = min(40, max(1, (int)current.bins[v1].size() * (int)current.bins[v1].size() / 15));
                for (int sp = 0; sp < sample_pairs && neighbor_cnt <= MAX_NEIGHBORS; sp++) {
                    int p1a = rand() % current.bins[v1].size();
                    int p1b = rand() % current.bins[v1].size();
                    if (p1a == p1b) continue;
                    
                    int oid1 = current.bins[v1][p1a];
                    int d1 = orders[oid1 - 1].d;
                    int c1 = orders[oid1 - 1].c;
                    int oid2 = current.bins[v1][p1b];
                    int d2 = orders[oid2 - 1].d;
                    int c2 = orders[oid2 - 1].c;
                    
                    for (int s_v2 = 0; s_v2 < samples && neighbor_cnt <= MAX_NEIGHBORS; s_v2++) {
                        int v2 = V[rand() % V.size()];
                        if (v1 == v2 || current.bins[v2].empty()) continue;
                        
                        int p2 = rand() % current.bins[v2].size();
                        if (++neighbor_cnt > MAX_NEIGHBORS) break;
                        
                        int oid3 = current.bins[v2][p2];
                        int d3 = orders[oid3 - 1].d;
                        int c3 = orders[oid3 - 1].c;

                        double new_total_cost = current.total_cost;
                        if (v1 == 0 && v2 > 0) new_total_cost += (c1 + c2 - c3);
                        else if (v1 > 0 && v2 == 0) new_total_cost += (c3 - c1 - c2);

                        double new_penalty = current.penalty 
                            - get_perf_penalty(v1, current.bin_load[v1]) 
                            - get_perf_penalty(v2, current.bin_load[v2])
                            + get_perf_penalty(v1, current.bin_load[v1] - d1 - d2 + d3) 
                            + get_perf_penalty(v2, current.bin_load[v2] - d3 + d1 + d2);

                        double new_fitness = -new_total_cost + alpha * new_penalty;
                        try_move(new_fitness, new_total_cost, new_penalty, (new_penalty < EPSILON),
                                 {"2-1", oid1, oid2, oid3, -1, v1, v2, TABU_TENURE}, v1, v2, p1a, p1b, p2, -1);
                    }
                }
            }
        }

        // ==================== MOVE 2-2 ====================
        else if (mt == "2-2") {
            int samples = min(10, (int)V.size());
            for (int s_v1 = 0; s_v1 < samples && neighbor_cnt <= MAX_NEIGHBORS; s_v1++) {
                int v1 = V[rand() % V.size()];
                if ((int)current.bins[v1].size() < 2) continue;
                
                int sample_pairs_v1 = min(30, max(1, (int)current.bins[v1].size() * (int)current.bins[v1].size() / 20));
                for (int sp1 = 0; sp1 < sample_pairs_v1 && neighbor_cnt <= MAX_NEIGHBORS; sp1++) {
                    int p1a = rand() % current.bins[v1].size();
                    int p1b = rand() % current.bins[v1].size();
                    if (p1a == p1b) continue;
                    
                    int oid1 = current.bins[v1][p1a];
                    int d1 = orders[oid1 - 1].d;
                    int c1 = orders[oid1 - 1].c;
                    int oid2 = current.bins[v1][p1b];
                    int d2 = orders[oid2 - 1].d;
                    int c2 = orders[oid2 - 1].c;
                    
                    for (int s_v2 = 0; s_v2 < samples && neighbor_cnt <= MAX_NEIGHBORS; s_v2++) {
                        int v2 = V[rand() % V.size()];
                        if (v1 >= v2 || (int)current.bins[v2].size() < 2) continue;
                        
                        int sample_pairs_v2 = min(30, max(1, (int)current.bins[v2].size() * (int)current.bins[v2].size() / 20));
                        for (int sp2 = 0; sp2 < sample_pairs_v2 && neighbor_cnt <= MAX_NEIGHBORS; sp2++) {
                            int p2a = rand() % current.bins[v2].size();
                            int p2b = rand() % current.bins[v2].size();
                            if (p2a == p2b) continue;
                            
                            if (++neighbor_cnt > MAX_NEIGHBORS) break;
                            
                            int oid3 = current.bins[v2][p2a];
                            int d3 = orders[oid3 - 1].d;
                            int c3 = orders[oid3 - 1].c;
                            int oid4 = current.bins[v2][p2b];
                            int d4 = orders[oid4 - 1].d;
                            int c4 = orders[oid4 - 1].c;

                            double new_total_cost = current.total_cost;
                            if (v1 == 0 && v2 > 0) new_total_cost += (c1 + c2 - c3 - c4);
                            else if (v1 > 0 && v2 == 0) new_total_cost += (c3 + c4 - c1 - c2);

                            double new_penalty = current.penalty 
                                - get_perf_penalty(v1, current.bin_load[v1]) 
                                - get_perf_penalty(v2, current.bin_load[v2])
                                + get_perf_penalty(v1, current.bin_load[v1] - d1 - d2 + d3 + d4) 
                                + get_perf_penalty(v2, current.bin_load[v2] - d3 - d4 + d1 + d2);

                            double new_fitness = -new_total_cost + alpha * new_penalty;
                            try_move(new_fitness, new_total_cost, new_penalty, (new_penalty < EPSILON),
                                     {"2-2", oid1, oid2, oid3, oid4, v1, v2, TABU_TENURE}, v1, v2, p1a, p1b, p2a, p2b);
                        }
                    }
                }
            }
        }

        // ==================== APPLY BEST MOVE ====================

        if (found) {
            // Áp dụng trực tiếp thông qua Delta Update O(1)
            if (mt == "1-0") {
                apply_1_0(current, best_v1, best_p1a, best_v2, final_cost, final_penalty, best_nf);
            } else if (mt == "1-1") {
                apply_1_1(current, best_v1, best_p1a, best_v2, best_p2a, final_cost, final_penalty, best_nf);
            } else if (mt == "2-0") {
                apply_2_0(current, best_v1, best_p1a, best_p1b, best_v2, final_cost, final_penalty, best_nf);
            } else if (mt == "2-1") {
                apply_2_1(current, best_v1, best_p1a, best_p1b, best_v2, best_p2a, final_cost, final_penalty, best_nf);
            } else if (mt == "2-2") {
                apply_2_2(current, best_v1, best_p1a, best_p1b, best_v2, best_p2a, best_p2b, final_cost, final_penalty, best_nf);
            }

            if (current.fitness < best.fitness - EPSILON) {
                best       = current;
                no_improve = 0;
                scorePi[mt_idx] += (improved ? delta1 : delta2);
            } else {
                no_improve++;
                scorePi[mt_idx] += delta3;
            }

            for (auto it = tabu_list.begin(); it != tabu_list.end(); ) {
                if (--(it->tenure) <= 0) it = tabu_list.erase(it);
                else                      ++it;
            }
            tabu_list.push_back(best_tm);

            TabuMove rev  = best_tm;
            rev.tenure    = TABU_TENURE;
            rev.v1        = best_tm.v2;
            rev.v2        = best_tm.v1;
            if (mt == "1-0") {
                rev.order1 = best_tm.order1;
            } else if (mt == "1-1") {
                swap(rev.order1, rev.order3);
            } else if (mt == "2-0") {
                rev.order1 = best_tm.order1;
                rev.order2 = best_tm.order2;
            } else if (mt == "2-1") {
                rev.order1 = best_tm.order3;
                rev.order2 = -1;
                rev.order3 = best_tm.order1;
                rev.order4 = best_tm.order2;
            } else if (mt == "2-2") {
                rev.order1 = best_tm.order3;
                rev.order2 = best_tm.order4;
                rev.order3 = best_tm.order1;
                rev.order4 = best_tm.order2;
            }
            tabu_list.push_back(rev);

        } else {
            no_improve++;
        }

        seg_count++;
        if (seg_count >= SEGMENT_LENGTH) {
            update_weights();
            seg_count = 0;
        }

        if (iter % 20 == 0 && iter > 0) {
            if (!current.is_feasible) {
                alpha = min(alpha * (1.0 + Beta), 1e7);
            } else {
                alpha = max(alpha * (1.0 - Beta * 0.2), 1.0);
            }
        }
    }

    return best;
}

int main(int argc, char* argv[]) {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    srand(42);
    auto start_time = chrono::high_resolution_clock::now();
    
    if (argc > 1) {
        string filename = argv[1];
        ifstream infile(filename);
        
        if (!infile.is_open()) {
            cerr << "Error: Cannot open file '" << filename << "'\n";
            return 1;
        }
        
        read_input(infile);
        infile.close();
    } else {
        read_input(cin);
    }
    
    Solution best = tabu_search();
    
    auto end_time = chrono::high_resolution_clock::now();
    double elapsed_ms = chrono::duration<double, milli>(end_time - start_time).count();
    
    output_solution(best, elapsed_ms);

    return 0;
}
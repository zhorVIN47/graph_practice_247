#pragma once
/*
 *  распределение Лапласа X 
 *
 * Неориентированный взвешенный граф без петель и кратных рёбер.
 * Веса целые w ∈ {1..50}; непрерывное распределение дискретизируется
 * через CDF: q_k = F(k+0.5) - F(k-0.5), затем P(w=k) = q_k / Q.
 */
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <queue>
#include <numeric>
#include <limits>
#include <cstdint>
#include <utility>

struct Edge {
    int u, v, w;
};

struct Graph {
    int V;
    std::vector<std::vector<std::pair<int, int>>> adj; // (сосед, вес)
    std::vector<Edge> edges;

    Graph() : V(0) {}
    explicit Graph(int v) : V(v), adj(v) {}

    // Неориентированное ребро: две записи в adj + одна в edges
    void add_edge(int u, int v, int w) {
        adj[u].push_back({v, w});
        adj[v].push_back({u, w});
        edges.push_back({u, v, w});
    }
};

// CDF распределения Лапласа: F(x) = P(X ≤ x)
inline double laplace_cdf(double x, double mu, double b) {
    if (b <= 0.0) b = 1e-12;
    if (x < mu) return 0.5 * std::exp((x - mu) / b);
    return 1.0 - 0.5 * std::exp(-(x - mu) / b);
}

/*
 * Дискретизация непрерывного Лапласа на носитель {1..50} :
 *   q_k = F(k + 0.5) - F(k - 0.5)
 *   Q   = sum q_k
 *   P(w = k) = q_k / Q
 * Индекс 0 не используется; P[1..50] — итоговые вероятности.
 */
inline std::vector<double> get_discrete_laplace(double mu, double b) {
    std::vector<double> P(51, 0.0);
    double Q = 0.0;
    for (int k = 1; k <= 50; ++k) {
        P[k] = laplace_cdf(k + 0.5, mu, b) - laplace_cdf(k - 0.5, mu, b);
        if (P[k] < 0.0) P[k] = 0.0;
        Q += P[k];
    }
    if (Q <= 0.0) {
        for (int k = 1; k <= 50; ++k) P[k] = 1.0 / 50.0;
        return P;
    }
    for (int k = 1; k <= 50; ++k) P[k] /= Q;
    return P;
}

/*
 * Модель G(V, p): для каждой пары i < j ребро появляется независимо с вероятностью p.
 * Вес: r ~ U(0,1), вес = минимальное k с r ≤ S_k, где S — накопленная сумма P(w).
 */
inline Graph generate_graph(int V, double p, const std::vector<double>& P_weights, uint32_t seed) {
    Graph g(V);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> U(0.0, 1.0);

    // Накопленные суммы S_k (ТЗ)
    std::vector<double> S(51, 0.0);
    for (int i = 1; i <= 50; ++i) S[i] = S[i - 1] + P_weights[i];
    S[50] = 1.0;

    for (int i = 0; i < V; ++i) {
        for (int j = i + 1; j < V; ++j) {
            if (U(rng) < p) {
                double r = U(rng);
                int w = 50;
                for (int k = 1; k <= 50; ++k) {
                    if (r <= S[k]) {
                        w = k;
                        break;
                    }
                }
                g.add_edge(i, j, w);
            }
        }
    }
    return g;
}

struct ComponentInfo {
    int num_components = 0;
    int lcc_size = 0;
    std::vector<int> component_of;
    int best_comp = -1;
};

inline ComponentInfo analyze_components(const Graph& g) {
    ComponentInfo info;
    info.component_of.assign(g.V, -1);
    int max_size = 0;

    for (int i = 0; i < g.V; ++i) {
        if (info.component_of[i] != -1) continue;
        std::queue<int> q;
        q.push(i);
        info.component_of[i] = info.num_components;
        int size = 0;
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            ++size;
            for (auto& e : g.adj[u]) {
                int v = e.first;
                if (info.component_of[v] == -1) {
                    info.component_of[v] = info.num_components;
                    q.push(v);
                }
            }
        }
        if (size > max_size) {
            max_size = size;
            info.best_comp = info.num_components;
        }
        ++info.num_components;
    }
    info.lcc_size = max_size;
    return info;
}

// Подграф наибольшей компоненты связности (LCC) с перенумерацией 0..size-1
inline Graph extract_lcc(const Graph& g, const ComponentInfo& info) {
    if (info.lcc_size == g.V) return g;

    std::vector<int> new_idx(g.V, -1);
    int cnt = 0;
    for (int i = 0; i < g.V; ++i) {
        if (info.component_of[i] == info.best_comp) new_idx[i] = cnt++;
    }

    Graph lcc(info.lcc_size);
    for (const auto& e : g.edges) {
        if (new_idx[e.u] != -1 && new_idx[e.v] != -1) {
            lcc.add_edge(new_idx[e.u], new_idx[e.v], e.w);
        }
    }
    return lcc;
}

inline Graph get_lcc(const Graph& g) {
    return extract_lcc(g, analyze_components(g));
}

// Базовые + LCC-характеристики одного графа 
struct GraphMetrics {
    int edges = 0;
    double density = 0.0;
    int num_components = 0;
    int lcc_size = 0;
    double lcc_ratio = 0.0;

    int deg_min = 0, deg_max = 0;
    double deg_avg = 0.0;

    int w_min = 0, w_max = 0, w_median = 0;
    double w_avg = 0.0;

    double avg_shortest_path = 0.0; // средняя длина КПут. (веса)
    int weighted_diameter = 0;      // max d(u,v) по весам
};

std::vector<int> dijkstra_dist(const Graph& g, int start);
GraphMetrics compute_metrics(const Graph& g);

/*
 * Задача 3: пары с большим топологическим расстоянием —
 * farthest pair двукратным BFS (невзвешенный hop-distance).
 */
inline std::pair<int, int> farthest_pair_bfs(const Graph& g) {
    if (g.V <= 1) return {0, 0};
    auto bfs = [&](int start) {
        std::vector<int> dist(g.V, -1);
        std::queue<int> q;
        dist[start] = 0;
        q.push(start);
        int far = start;
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            if (dist[u] > dist[far]) far = u;
            for (auto& e : g.adj[u]) {
                int v = e.first;
                if (dist[v] == -1) {
                    dist[v] = dist[u] + 1;
                    q.push(v);
                }
            }
        }
        return std::make_pair(far, dist[far]);
    };
    auto a = bfs(0).first;
    auto b = bfs(a).first;
    return {a, b};
}

// Результат обработки одного графа (метрики + все 4 задачи)
struct SingleGraphResult {
    GraphMetrics metrics;
    double t_kruskal = 0, t_prim = 0, t_boruvka = 0;
    int mst_kruskal = 0, mst_prim = 0, mst_boruvka = 0;
    bool mst_agree = false;

    double t_fw = 0, t_dij_all = 0, t_johnson = 0;
    double avg_fw = 0, avg_dij = 0, avg_johnson = 0;
    bool apsp_agree = false; // совпадение ВСЕХ попарных расстояний (не только средних)

    double t_dij_pair = 0, t_bidir = 0;
    int path_dij = 0, path_bidir = 0;
    bool pair_agree = false;

    double t_ek = 0, t_dinic = 0, t_pr = 0;
    int flow_ek = 0, flow_dinic = 0, flow_pr = 0;
    bool flow_agree = false;
};

SingleGraphResult process_graph(const Graph& g);

#include "graph_core.h"
#include <chrono>
#include <functional>

// --- Задача 1: MST (Краскал / Прим / Борувка) ---
// --- Задача 2–3: кратчайшие пути ---
// --- Задача 4: максимальный поток (Эдмондс–Карп / Диниц / Push–Relabel) ---

static constexpr int INF = 1000000000;

struct DSU {
    std::vector<int> p, r;
    explicit DSU(int n) : p(n), r(n, 0) { std::iota(p.begin(), p.end(), 0); }
    int find(int x) { return p[x] == x ? x : p[x] = find(p[x]); }
    bool unite(int x, int y) {
        x = find(x);
        y = find(y);
        if (x == y) return false;
        if (r[x] < r[y]) std::swap(x, y);
        p[y] = x;
        if (r[x] == r[y]) ++r[x];
        return true;
    }
};

int kruskal(const Graph& g) {
    auto edges = g.edges;
    std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) { return a.w < b.w; });
    DSU dsu(g.V);
    int mst_w = 0;
    for (const auto& e : edges) {
        if (dsu.unite(e.u, e.v)) mst_w += e.w;
    }
    return mst_w;
}

int prim(const Graph& g) {
    if (g.V == 0) return 0;
    std::vector<bool> in_mst(g.V, false);
    std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<>> pq;
    pq.push({0, 0});
    int mst_w = 0;
    int taken = 0;
    while (!pq.empty() && taken < g.V) {
        auto [w, u] = pq.top();
        pq.pop();
        if (in_mst[u]) continue;
        in_mst[u] = true;
        mst_w += w;
        ++taken;
        for (auto& [v, weight] : g.adj[u]) {
            if (!in_mst[v]) pq.push({weight, v});
        }
    }
    return mst_w;
}

int boruvka(const Graph& g) {
    if (g.V == 0) return 0;
    DSU dsu(g.V);
    int mst_w = 0;
    int num_comp = g.V;
    while (num_comp > 1) {
        std::vector<int> cheapest(g.V, -1);
        for (int i = 0; i < static_cast<int>(g.edges.size()); ++i) {
            const auto& e = g.edges[i];
            int su = dsu.find(e.u), sv = dsu.find(e.v);
            if (su == sv) continue;
            if (cheapest[su] == -1 || g.edges[cheapest[su]].w > e.w) cheapest[su] = i;
            if (cheapest[sv] == -1 || g.edges[cheapest[sv]].w > e.w) cheapest[sv] = i;
        }
        bool progressed = false;
        std::vector<char> used(g.edges.size(), 0);
        for (int i = 0; i < g.V; ++i) {
            int idx = cheapest[i];
            if (idx == -1 || used[idx]) continue;
            used[idx] = 1;
            const auto& e = g.edges[idx];
            if (dsu.unite(e.u, e.v)) {
                mst_w += e.w;
                --num_comp;
                progressed = true;
            }
        }
        if (!progressed) break;
    }
    return mst_w;
}

std::vector<int> dijkstra_dist(const Graph& g, int start) {
    std::vector<int> dist(g.V, INF);
    if (g.V == 0) return dist;
    std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<>> pq;
    dist[start] = 0;
    pq.push({0, start});
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d > dist[u]) continue;
        for (auto& [v, w] : g.adj[u]) {
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                pq.push({dist[v], v});
            }
        }
    }
    return dist;
}

GraphMetrics compute_metrics(const Graph& g) {
    GraphMetrics m;
    auto info = analyze_components(g);
    m.edges = static_cast<int>(g.edges.size());
    double max_e = static_cast<double>(g.V) * (g.V - 1) / 2.0;
    m.density = max_e > 0 ? m.edges / max_e : 0.0;
    m.num_components = info.num_components;
    m.lcc_size = info.lcc_size;
    m.lcc_ratio = g.V > 0 ? static_cast<double>(info.lcc_size) / g.V : 0.0;

    Graph lcc = extract_lcc(g, info);
    if (lcc.V == 0) return m;

    std::vector<int> deg(lcc.V, 0);
    for (int i = 0; i < lcc.V; ++i) deg[i] = static_cast<int>(lcc.adj[i].size());
    m.deg_min = *std::min_element(deg.begin(), deg.end());
    m.deg_max = *std::max_element(deg.begin(), deg.end());
    m.deg_avg = std::accumulate(deg.begin(), deg.end(), 0.0) / lcc.V;

    if (!lcc.edges.empty()) {
        std::vector<int> ws;
        ws.reserve(lcc.edges.size());
        long long sum_w = 0;
        for (const auto& e : lcc.edges) {
            ws.push_back(e.w);
            sum_w += e.w;
        }
        std::sort(ws.begin(), ws.end());
        m.w_min = ws.front();
        m.w_max = ws.back();
        m.w_avg = static_cast<double>(sum_w) / ws.size();
        m.w_median = ws[ws.size() / 2];
    }
    // avg_shortest_path / weighted_diameter заполняются в process_graph после APSP
    return m;
}

int dijkstra_pair(const Graph& g, int s, int t) {
    if (s < 0 || t < 0 || s >= g.V || t >= g.V) return INF;
    return dijkstra_dist(g, s)[t];
}

int bidir_dijkstra(const Graph& g, int s, int t) {
    if (s == t) return 0;
    if (g.V == 0) return INF;
    std::vector<int> dist_f(g.V, INF), dist_b(g.V, INF);
    std::vector<char> vis_f(g.V, 0), vis_b(g.V, 0);
    using P = std::pair<int, int>;
    std::priority_queue<P, std::vector<P>, std::greater<>> pq_f, pq_b;
    dist_f[s] = 0;
    dist_b[t] = 0;
    pq_f.push({0, s});
    pq_b.push({0, t});
    int mu = INF;

    auto expand = [&](auto& pq, std::vector<int>& dist, std::vector<char>& vis,
                      const std::vector<int>& other_dist) {
        if (pq.empty()) return;
        auto [d, u] = pq.top();
        pq.pop();
        if (vis[u]) return;
        if (d > dist[u]) return;
        vis[u] = 1;
        if (other_dist[u] < INF) mu = std::min(mu, dist[u] + other_dist[u]);
        for (auto& [v, w] : g.adj[u]) {
            if (dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                pq.push({dist[v], v});
            }
        }
    };

    while (!pq_f.empty() || !pq_b.empty()) {
        if (!pq_f.empty() && (pq_b.empty() || pq_f.top().first <= pq_b.top().first)) {
            expand(pq_f, dist_f, vis_f, dist_b);
        } else {
            expand(pq_b, dist_b, vis_b, dist_f);
        }
        int top_f = pq_f.empty() ? INF : pq_f.top().first;
        int top_b = pq_b.empty() ? INF : pq_b.top().first;
        if (top_f + top_b >= mu) break;
    }
    return mu;
}

std::vector<std::vector<int>> floyd_warshall(const Graph& g) {
    std::vector<std::vector<int>> dist(g.V, std::vector<int>(g.V, INF));
    for (int i = 0; i < g.V; ++i) dist[i][i] = 0;
    for (const auto& e : g.edges) {
        dist[e.u][e.v] = std::min(dist[e.u][e.v], e.w);
        dist[e.v][e.u] = std::min(dist[e.v][e.u], e.w);
    }
    for (int k = 0; k < g.V; ++k) {
        for (int i = 0; i < g.V; ++i) {
            if (dist[i][k] >= INF) continue;
            for (int j = 0; j < g.V; ++j) {
                if (dist[k][j] >= INF) continue;
                int nd = dist[i][k] + dist[k][j];
                if (nd < dist[i][j]) dist[i][j] = nd;
            }
        }
    }
    return dist;
}

// Вспомогательная статистика по матрице кратчайших путей
struct ApspStats {
    double avg = 0.0;
    int diameter = 0;
    int far_s = 0, far_t = 0; // пара с max взвешенным расстоянием (для max-flow)
    bool matches_ref = true;
};

static ApspStats stats_from_matrix(const std::vector<std::vector<int>>& dist) {
    ApspStats st;
    long long sum = 0;
    long long pairs = 0;
    int n = static_cast<int>(dist.size());
    int best = -1;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (dist[i][j] < INF) {
                sum += dist[i][j];
                ++pairs;
                if (dist[i][j] > best) {
                    best = dist[i][j];
                    st.far_s = i;
                    st.far_t = j;
                }
            }
        }
    }
    st.avg = pairs > 0 ? static_cast<double>(sum) / pairs : 0.0;
    st.diameter = best < 0 ? 0 : best;
    return st;
}

/*
 * Дейкстра из каждой вершины + поэлементная сверка с эталонной матрицей (Флойд)
 * фиксируем совпадение найденных расстояний у всех трёх подходов
 */
static ApspStats dijkstra_apsp_checked(const Graph& g, const std::vector<std::vector<int>>& ref) {
    ApspStats st;
    long long sum = 0;
    long long pairs = 0;
    int best = -1;
    for (int i = 0; i < g.V; ++i) {
        auto d = dijkstra_dist(g, i);
        for (int j = 0; j < g.V; ++j) {
            if (d[j] != ref[i][j]) st.matches_ref = false;
        }
        for (int j = i + 1; j < g.V; ++j) {
            if (d[j] < INF) {
                sum += d[j];
                ++pairs;
                if (d[j] > best) {
                    best = d[j];
                    st.far_s = i;
                    st.far_t = j;
                }
            }
        }
    }
    st.avg = pairs > 0 ? static_cast<double>(sum) / pairs : 0.0;
    st.diameter = best < 0 ? 0 : best;
    return st;
}

/*
 * Алгоритм Джонсона (полный цикл):
 * 1) фиктивная q, рёбра веса 0; Bellman–Ford → потенциалы h[v]
 * 2) перевзвешивание w'(u,v) = w(u,v) + h[u] - h[v] ≥ 0
 * 3) Дейкстра из каждой вершины; восстановление d = d' - h[u] + h[v]
 * При w >= 1 потенциалы обычно нулевые, но шаги выполняются явно.
 */
static ApspStats johnson_apsp_checked(const Graph& g, const std::vector<std::vector<int>>& ref) {
    ApspStats st;
    int n = g.V;
    if (n == 0) return st;

    std::vector<Edge> edges = g.edges;
    for (int i = 0; i < n; ++i) edges.push_back({n, i, 0}); // q = n

    std::vector<int> h(n + 1, INF);
    h[n] = 0;
    for (int iter = 0; iter < n; ++iter) {
        bool upd = false;
        for (const auto& e : edges) {
            if (h[e.u] < INF && h[e.u] + e.w < h[e.v]) {
                h[e.v] = h[e.u] + e.w;
                upd = true;
            }
            if (e.u != n && h[e.v] < INF && h[e.v] + e.w < h[e.u]) {
                h[e.u] = h[e.v] + e.w;
                upd = true;
            }
        }
        if (!upd) break;
    }

    std::vector<std::vector<std::pair<int, int>>> adj(n);
    for (const auto& e : g.edges) {
        adj[e.u].push_back({e.v, e.w + h[e.u] - h[e.v]});
        adj[e.v].push_back({e.u, e.w + h[e.v] - h[e.u]});
    }

    auto dijk = [&](int start) {
        std::vector<int> dist(n, INF);
        std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<>> pq;
        dist[start] = 0;
        pq.push({0, start});
        while (!pq.empty()) {
            auto [d, u] = pq.top();
            pq.pop();
            if (d > dist[u]) continue;
            for (auto& [v, w] : adj[u]) {
                if (dist[u] + w < dist[v]) {
                    dist[v] = dist[u] + w;
                    pq.push({dist[v], v});
                }
            }
        }
        return dist;
    };

    long long sum = 0;
    long long pairs = 0;
    int best = -1;
    for (int i = 0; i < n; ++i) {
        auto dp = dijk(i);
        for (int j = 0; j < n; ++j) {
            int real = (dp[j] >= INF) ? INF : (dp[j] - h[i] + h[j]);
            if (real != ref[i][j]) st.matches_ref = false;
            if (j > i && real < INF) {
                sum += real;
                ++pairs;
                if (real > best) {
                    best = real;
                    st.far_s = i;
                    st.far_t = j;
                }
            }
        }
    }
    st.avg = pairs > 0 ? static_cast<double>(sum) / pairs : 0.0;
    st.diameter = best < 0 ? 0 : best;
    return st;
}

// Максимальный поток
struct FlowEdge {
    int to, rev, cap, flow;
};

struct FlowNetwork {
    std::vector<std::vector<FlowEdge>> adj;
    explicit FlowNetwork(int n) : adj(n) {}

    void add_directed(int u, int v, int cap) {
        FlowEdge a{v, static_cast<int>(adj[v].size()), cap, 0};
        FlowEdge b{u, static_cast<int>(adj[u].size()), 0, 0};
        adj[u].push_back(a);
        adj[v].push_back(b);
    }
};

/*
 * Сеть для max-flow: неориентированное ребро (u,v) веса w
 * две направленные дуги u→v и v→u с пропускной способностью w
 * В residual-графе у каждой дуги есть обратная с начальной ёмкостью 0
 */
FlowNetwork build_flow_network(const Graph& g) {
    FlowNetwork net(g.V);
    for (const auto& e : g.edges) {
        net.add_directed(e.u, e.v, e.w);
        net.add_directed(e.v, e.u, e.w);
    }
    return net;
}

int edmonds_karp(FlowNetwork net, int s, int t) {
    int n = static_cast<int>(net.adj.size());
    int max_flow = 0;
    while (true) {
        std::vector<int> parent_v(n, -1), parent_e(n, -1);
        std::queue<int> q;
        q.push(s);
        parent_v[s] = s;
        while (!q.empty() && parent_v[t] == -1) {
            int u = q.front();
            q.pop();
            for (int i = 0; i < static_cast<int>(net.adj[u].size()); ++i) {
                auto& e = net.adj[u][i];
                if (parent_v[e.to] == -1 && e.cap - e.flow > 0) {
                    parent_v[e.to] = u;
                    parent_e[e.to] = i;
                    q.push(e.to);
                }
            }
        }
        if (parent_v[t] == -1) break;
        int push = INF;
        for (int v = t; v != s; v = parent_v[v]) {
            auto& e = net.adj[parent_v[v]][parent_e[v]];
            push = std::min(push, e.cap - e.flow);
        }
        for (int v = t; v != s; v = parent_v[v]) {
            int u = parent_v[v];
            auto& e = net.adj[u][parent_e[v]];
            e.flow += push;
            net.adj[v][e.rev].flow -= push;
        }
        max_flow += push;
    }
    return max_flow;
}

int dinic(FlowNetwork net, int s, int t) {
    int n = static_cast<int>(net.adj.size());
    int max_flow = 0;
    std::vector<int> level(n), ptr(n);

    auto bfs = [&]() {
        std::fill(level.begin(), level.end(), -1);
        level[s] = 0;
        std::queue<int> q;
        q.push(s);
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            for (auto& e : net.adj[u]) {
                if (e.cap - e.flow > 0 && level[e.to] == -1) {
                    level[e.to] = level[u] + 1;
                    q.push(e.to);
                }
            }
        }
        return level[t] != -1;
    };

    std::function<int(int, int)> dfs = [&](int u, int pushed) -> int {
        if (pushed == 0 || u == t) return pushed;
        for (int& cid = ptr[u]; cid < static_cast<int>(net.adj[u].size()); ++cid) {
            auto& e = net.adj[u][cid];
            if (level[u] + 1 != level[e.to] || e.cap - e.flow == 0) continue;
            int tr = dfs(e.to, std::min(pushed, e.cap - e.flow));
            if (!tr) continue;
            e.flow += tr;
            net.adj[e.to][e.rev].flow -= tr;
            return tr;
        }
        return 0;
    };

    while (bfs()) {
        std::fill(ptr.begin(), ptr.end(), 0);
        while (int pushed = dfs(s, INF)) max_flow += pushed;
    }
    return max_flow;
}

int push_relabel(FlowNetwork net, int s, int t) {
    int n = static_cast<int>(net.adj.size());
    if (n == 0 || s == t) return 0;
    std::vector<long long> excess(n, 0);
    std::vector<int> height(n, 0);
    std::vector<int> seen(n, 0);
    height[s] = n;

    auto push = [&](int u, FlowEdge& e) {
        long long amt = std::min(excess[u], static_cast<long long>(e.cap - e.flow));
        if (amt <= 0) return;
        e.flow += static_cast<int>(amt);
        net.adj[e.to][e.rev].flow -= static_cast<int>(amt);
        excess[u] -= amt;
        excess[e.to] += amt;
    };

    for (auto& e : net.adj[s]) {
        if (e.cap > 0) {
            excess[s] += e.cap; // чтобы push сработал от s
            push(s, e);
        }
    }
    excess[s] = 0;

    auto relabel = [&](int u) {
        int minh = INF;
        for (auto& e : net.adj[u]) {
            if (e.cap - e.flow > 0) minh = std::min(minh, height[e.to]);
        }
        if (minh < INF) height[u] = minh + 1;
    };

    std::queue<int> q;
    std::vector<char> inq(n, 0);
    for (int i = 0; i < n; ++i) {
        if (i != s && i != t && excess[i] > 0) {
            q.push(i);
            inq[i] = 1;
        }
    }

    while (!q.empty()) {
        int u = q.front();
        q.pop();
        inq[u] = 0;
        while (excess[u] > 0) {
            if (seen[u] < static_cast<int>(net.adj[u].size())) {
                auto& e = net.adj[u][seen[u]];
                if (e.cap - e.flow > 0 && height[u] == height[e.to] + 1) {
                    long long before = excess[e.to];
                    push(u, e);
                    if (e.to != s && e.to != t && before == 0 && excess[e.to] > 0 && !inq[e.to]) {
                        q.push(e.to);
                        inq[e.to] = 1;
                    }
                } else {
                    ++seen[u];
                }
            } else {
                relabel(u);
                seen[u] = 0;
                break;
            }
        }
        if (excess[u] > 0 && !inq[u]) {
            q.push(u);
            inq[u] = 1;
        }
    }
    return static_cast<int>(excess[t]);
}

using Clock = std::chrono::high_resolution_clock;

template <typename F>
double time_ms(F&& f) {
    auto t0 = Clock::now();
    f();
    auto t1 = Clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

/*
 * Полный цикл по одному графу :
 * — метрики на всём графе / LCC
 * — Задача 1: MST (Краскал / Прим / Борувка)
 * — Задача 2: APSP (Флойд / Дейкстра×V / Джонсон) + сверка матриц расстояний
 * — Задача 3: путь между парой с max топологическим расстоянием (BFS)
 * — Задача 4: max-flow; s,t — пара с max взвешенным кратчайшим расстоянием
 */
SingleGraphResult process_graph(const Graph& g) {
    SingleGraphResult r;
    r.metrics = compute_metrics(g);
    Graph lcc = get_lcc(g);

    // --- Задача 1: MST ---
    r.t_kruskal = time_ms([&] { r.mst_kruskal = kruskal(lcc); });
    r.t_prim = time_ms([&] { r.mst_prim = prim(lcc); });
    r.t_boruvka = time_ms([&] { r.mst_boruvka = boruvka(lcc); });
    r.mst_agree = (r.mst_kruskal == r.mst_prim && r.mst_prim == r.mst_boruvka);

    // Задача 2: APSP 
    // Эталон — матрица Флойда; остальные сверяются поэлементно.
    std::vector<std::vector<int>> fw_dist;
    r.t_fw = time_ms([&] {
        fw_dist = floyd_warshall(lcc);
        ApspStats st = stats_from_matrix(fw_dist);
        r.avg_fw = st.avg;
    });

    ApspStats dij_st;
    r.t_dij_all = time_ms([&] { dij_st = dijkstra_apsp_checked(lcc, fw_dist); });
    r.avg_dij = dij_st.avg;
    r.metrics.avg_shortest_path = dij_st.avg;
    r.metrics.weighted_diameter = dij_st.diameter;

    ApspStats joh_st;
    r.t_johnson = time_ms([&] { joh_st = johnson_apsp_checked(lcc, fw_dist); });
    r.avg_johnson = joh_st.avg;

    // Совпадение расстояний 
    r.apsp_agree = dij_st.matches_ref && joh_st.matches_ref;

    //  Задача 3: кратчайший путь между двумя вершинами 
    // Правило выбора пары одинаково для всех графов: farthest pair по BFS.
    auto [pair_s, pair_t] = farthest_pair_bfs(lcc);
    r.t_dij_pair = time_ms([&] { r.path_dij = dijkstra_pair(lcc, pair_s, pair_t); });
    r.t_bidir = time_ms([&] { r.path_bidir = bidir_dijkstra(lcc, pair_s, pair_t); });
    r.pair_agree = (r.path_dij == r.path_bidir);

    // Задача 4: максимальный поток 
    //  s,t — вершины с наибольшим кратчайшим (взвешенным) расстоянием в LCC.
    int flow_s = dij_st.far_s;
    int flow_t = dij_st.far_t;
    auto net_base = build_flow_network(lcc);
    r.t_ek = time_ms([&] { r.flow_ek = edmonds_karp(net_base, flow_s, flow_t); });
    r.t_dinic = time_ms([&] { r.flow_dinic = dinic(net_base, flow_s, flow_t); });
    r.t_pr = time_ms([&] { r.flow_pr = push_relabel(net_base, flow_s, flow_t); });
    r.flow_agree = (r.flow_ek == r.flow_dinic && r.flow_dinic == r.flow_pr);

    return r;
}

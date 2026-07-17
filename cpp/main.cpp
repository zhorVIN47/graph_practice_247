/*
 * CLI-ядро (ТЗ: Python ↔ C++ через JSON).
 *
 * Вход:  params.json
 *   { "mode": "probs"|"experiment", "mu", "b", "p", "V", "num_graphs", "base_seed" }
 * Выход: result.json  — те же массивы, что раньше отдавал pybind.
 *
 * Сборка: см. build.ps1 / CMakeLists.txt (без pybind11).
 */
#include "graph_core.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// --- Минимальный разбор плоского JSON (только наши ключи) ---
std::string read_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Не удалось открыть: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Ищет "key": <number|string> в тексте (достаточно для нашего params.json)
double json_get_number(const std::string& s, const std::string& key, double def) {
    std::string pat = "\"" + key + "\"";
    auto pos = s.find(pat);
    if (pos == std::string::npos) return def;
    pos = s.find(':', pos);
    if (pos == std::string::npos) return def;
    ++pos;
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) ++pos;
    try {
        return std::stod(s.substr(pos));
    } catch (...) {
        return def;
    }
}

std::string json_get_string(const std::string& s, const std::string& key, const std::string& def) {
    std::string pat = "\"" + key + "\"";
    auto pos = s.find(pat);
    if (pos == std::string::npos) return def;
    pos = s.find(':', pos);
    if (pos == std::string::npos) return def;
    pos = s.find('"', pos);
    if (pos == std::string::npos) return def;
    auto end = s.find('"', pos + 1);
    if (end == std::string::npos) return def;
    return s.substr(pos + 1, end - pos - 1);
}

// --- Запись JSON ---
struct JsonWriter {
    std::ostream& out;
    bool first_field = true;

    explicit JsonWriter(std::ostream& o) : out(o) {}

    void begin_obj() { out << "{"; first_field = true; }
    void end_obj() { out << "}"; }

    void key(const char* k) {
        if (!first_field) out << ",";
        first_field = false;
        out << "\"" << k << "\":";
    }

    void number(double x) {
        if (std::isfinite(x)) out << std::setprecision(17) << x;
        else out << "null";
    }

    void boolean(bool v) { out << (v ? "true" : "false"); }

    template <typename T>
    void array_num(const std::vector<T>& v) {
        out << "[";
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) out << ",";
            out << std::setprecision(17) << v[i];
        }
        out << "]";
    }

    void array_bool(const std::vector<char>& v) {
        out << "[";
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) out << ",";
            out << (v[i] ? "true" : "false");
        }
        out << "]";
    }
};

void write_probs_json(const std::string& path, const std::vector<double>& P) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Не удалось записать: " + path);
    JsonWriter j(out);
    j.begin_obj();
    j.key("probs");
    std::vector<double> probs(50);
    for (int k = 1; k <= 50; ++k) probs[k - 1] = P[k];
    j.array_num(probs);
    j.end_obj();
    out << "\n";
}

void run_and_write_experiment(
    const std::string& path,
    double mu, double b, double p,
    int V, int num_graphs, int base_seed
) {
    auto P = get_discrete_laplace(mu, b);

    std::vector<double> probs(50);
    for (int k = 1; k <= 50; ++k) probs[k - 1] = P[k];

    std::vector<int> edges, num_comp, lcc_size, deg_min, deg_max;
    std::vector<int> w_min, w_median, w_max, diameter;
    std::vector<double> density, lcc_ratio, deg_avg, w_avg, avg_sp;
    std::vector<double> t_kruskal, t_prim, t_boruvka, t_fw, t_dij_all, t_johnson;
    std::vector<double> t_dij_pair, t_bidir, t_ek, t_dinic, t_pr;
    std::vector<double> avg_fw, avg_dij, avg_johnson;
    std::vector<int> mst_w, path_len, flow_val;
    std::vector<char> mst_agree, apsp_agree, pair_agree, flow_agree;

    for (int g_idx = 0; g_idx < num_graphs; ++g_idx) {
        uint32_t seed = static_cast<uint32_t>(
            base_seed + g_idx * 9973 + static_cast<int>(p * 10000) + static_cast<int>(mu * 100));
        Graph g = generate_graph(V, p, P, seed);
        SingleGraphResult r = process_graph(g);

        edges.push_back(r.metrics.edges);
        density.push_back(r.metrics.density);
        num_comp.push_back(r.metrics.num_components);
        lcc_size.push_back(r.metrics.lcc_size);
        lcc_ratio.push_back(r.metrics.lcc_ratio);
        deg_min.push_back(r.metrics.deg_min);
        deg_avg.push_back(r.metrics.deg_avg);
        deg_max.push_back(r.metrics.deg_max);
        w_min.push_back(r.metrics.w_min);
        w_avg.push_back(r.metrics.w_avg);
        w_median.push_back(r.metrics.w_median);
        w_max.push_back(r.metrics.w_max);
        avg_sp.push_back(r.metrics.avg_shortest_path);
        diameter.push_back(r.metrics.weighted_diameter);

        t_kruskal.push_back(r.t_kruskal);
        t_prim.push_back(r.t_prim);
        t_boruvka.push_back(r.t_boruvka);
        mst_w.push_back(r.mst_kruskal);
        mst_agree.push_back(r.mst_agree);

        t_fw.push_back(r.t_fw);
        t_dij_all.push_back(r.t_dij_all);
        t_johnson.push_back(r.t_johnson);
        avg_fw.push_back(r.avg_fw);
        avg_dij.push_back(r.avg_dij);
        avg_johnson.push_back(r.avg_johnson);
        apsp_agree.push_back(r.apsp_agree);

        t_dij_pair.push_back(r.t_dij_pair);
        t_bidir.push_back(r.t_bidir);
        path_len.push_back(r.path_dij);
        pair_agree.push_back(r.pair_agree);

        t_ek.push_back(r.t_ek);
        t_dinic.push_back(r.t_dinic);
        t_pr.push_back(r.t_pr);
        flow_val.push_back(r.flow_ek);
        flow_agree.push_back(r.flow_agree);

        // Прогресс в stderr (не портит JSON)
        std::cerr << "[graph_core] graph " << (g_idx + 1) << "/" << num_graphs << "\n";
    }

    std::ofstream out(path);
    if (!out) throw std::runtime_error("Не удалось записать: " + path);
    JsonWriter j(out);
    j.begin_obj();

    auto put_d = [&](const char* k, const std::vector<double>& v) { j.key(k); j.array_num(v); };
    auto put_i = [&](const char* k, const std::vector<int>& v) { j.key(k); j.array_num(v); };
    auto put_b = [&](const char* k, const std::vector<char>& v) { j.key(k); j.array_bool(v); };

    j.key("probs"); j.array_num(probs);
    put_i("edges", edges);
    put_d("density", density);
    put_i("num_components", num_comp);
    put_i("lcc_size", lcc_size);
    put_d("lcc_ratio", lcc_ratio);
    put_i("deg_min", deg_min);
    put_d("deg_avg", deg_avg);
    put_i("deg_max", deg_max);
    put_i("w_min", w_min);
    put_d("w_avg", w_avg);
    put_i("w_median", w_median);
    put_i("w_max", w_max);
    put_d("avg_shortest_path", avg_sp);
    put_i("weighted_diameter", diameter);

    put_d("mst_kruskal", t_kruskal);
    put_d("mst_prim", t_prim);
    put_d("mst_boruvka", t_boruvka);
    put_i("mst_weight", mst_w);
    put_b("mst_agree", mst_agree);

    put_d("fw", t_fw);
    put_d("dij_all", t_dij_all);
    put_d("johnson", t_johnson);
    put_d("avg_fw", avg_fw);
    put_d("avg_dij", avg_dij);
    put_d("avg_johnson", avg_johnson);
    put_b("apsp_agree", apsp_agree);

    put_d("dij_pair", t_dij_pair);
    put_d("bidir", t_bidir);
    put_i("path_len", path_len);
    put_b("pair_agree", pair_agree);

    put_d("ek", t_ek);
    put_d("dinic", t_dinic);
    put_d("pr", t_pr);
    put_i("flow_value", flow_val);
    put_b("flow_agree", flow_agree);

    j.end_obj();
    out << "\n";
}

void print_usage() {
    std::cerr
        << "Usage:\n"
        << "  graph_core.exe --params params.json --out result.json\n"
        << "params.json fields:\n"
        << "  mode: \"probs\" | \"experiment\"\n"
        << "  mu, b, p, V, num_graphs, base_seed\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string params_path, out_path;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--params") == 0 && i + 1 < argc) params_path = argv[++i];
        else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) out_path = argv[++i];
        else if (std::strcmp(argv[i], "--help") == 0) { print_usage(); return 0; }
    }
    if (params_path.empty() || out_path.empty()) {
        print_usage();
        return 1;
    }

    try {
        std::string raw = read_file(params_path);
        std::string mode = json_get_string(raw, "mode", "experiment");
        double mu = json_get_number(raw, "mu", 25.0);
        double b = json_get_number(raw, "b", 5.0);
        double p = json_get_number(raw, "p", 0.15);
        int V = static_cast<int>(json_get_number(raw, "V", 500));
        int num_graphs = static_cast<int>(json_get_number(raw, "num_graphs", 50));
        int base_seed = static_cast<int>(json_get_number(raw, "base_seed", 42));

        if (mode == "probs") {
            auto P = get_discrete_laplace(mu, b);
            write_probs_json(out_path, P);
        } else {
            if (V < 2 || num_graphs < 1) {
                throw std::runtime_error("V >= 2 и num_graphs >= 1");
            }
            run_and_write_experiment(out_path, mu, b, p, V, num_graphs, base_seed);
        }
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 2;
    }
    return 0;
}

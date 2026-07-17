from __future__ import annotations

import json
import os
import subprocess
import tempfile
from typing import Any

import numpy as np

_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
_CPP = os.path.join(_ROOT, "cpp")
_IO_DIR = os.path.join(_ROOT, "io")  

# Ищем скомпилированный бинарник
_EXE_CANDIDATES = [
    os.path.join(_CPP, "graph_core.exe"),
    os.path.join(_CPP, "build", "graph_core.exe"),
    os.path.join(_CPP, "graph_core"),
    os.path.join(_CPP, "build", "graph_core"),
]


def _find_exe() -> str | None:
    for p in _EXE_CANDIDATES:
        if os.path.isfile(p):
            return p
    return None


_EXE = _find_exe()
_IMPORT_ERROR = None if _EXE else (
    "Не найден graph_core.exe. Соберите: powershell -File cpp\\build.ps1"
)

TZ_V = 500
TZ_NUM_GRAPHS = 50
P_VALUES = [0.05, 0.15, 0.4]

PRESETS = [
    {"name": "Узкий пик", "mu": 25.0, "b": 1.5, "desc": "масса около w=25"},
    {"name": "Широкий", "mu": 25.0, "b": 10.0, "desc": "размазанные веса"},
    {"name": "Сдвиг влево", "mu": 8.0, "b": 5.0, "desc": "чаще малые веса"},
]


def core_available() -> tuple[bool, str | None]:
    global _EXE
    _EXE = _find_exe()
    if _EXE:
        return True, None
    return False, _IMPORT_ERROR


def _run_cpp(params: dict[str, Any]) -> dict[str, Any]:

    ok, err = core_available()
    if not ok:
        raise RuntimeError(err)

    os.makedirs(_IO_DIR, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=_IO_DIR) as tmp:
        params_path = os.path.join(tmp, "params.json")
        out_path = os.path.join(tmp, "result.json")
        with open(params_path, "w", encoding="utf-8") as f:
            json.dump(params, f, ensure_ascii=False, indent=2)

        # timeout большой: полный прогон V=500 может идти часами
        proc = subprocess.run(
            [_EXE, "--params", params_path, "--out", out_path],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=None,
        )
        if proc.returncode != 0:
            raise RuntimeError(
                f"graph_core.exe failed ({proc.returncode}):\n{proc.stderr or proc.stdout}"
            )
        with open(out_path, encoding="utf-8") as f:
            return json.load(f)


def series_stats(arr: np.ndarray) -> dict[str, float]:
    """Агрегаты по выборке из 50 графов (ТЗ)."""
    if arr.size == 0:
        return {"mean": 0.0, "median": 0.0, "min": 0.0, "max": 0.0}
    return {
        "mean": float(np.mean(arr)),
        "median": float(np.median(arr)),
        "min": float(np.min(arr)),
        "max": float(np.max(arr)),
    }


def algo_time_stats(
    times: np.ndarray,
    names: list[str],
    result_values: np.ndarray | None = None,
) -> dict[str, Any]:
   
    n = times.shape[0]
    means = np.mean(times, axis=1)
    medians = np.median(times, axis=1)
    mins = np.min(times, axis=1)
    maxs = np.max(times, axis=1)
    winners = np.argmin(times, axis=0)
    wins = {names[i]: int(np.sum(winners == i)) for i in range(n)}
    out: dict[str, Any] = {
        "names": names,
        "means": means.tolist(),
        "medians": medians.tolist(),
        "mins": mins.tolist(),
        "maxs": maxs.tolist(),
        "wins": wins,
    }
    if result_values is not None:
        out["result_mean"] = float(np.mean(result_values))
        out["result_median"] = float(np.median(result_values))
    return out


def summarize_raw(raw: dict, V: int) -> dict[str, Any]:
    metric_keys = [
        "edges", "density", "num_components", "lcc_size", "lcc_ratio",
        "deg_min", "deg_avg", "deg_max",
        "w_min", "w_avg", "w_median", "w_max",
        "avg_shortest_path", "weighted_diameter",
    ]
    graph_stats = {k: series_stats(np.asarray(raw[k], dtype=float)) for k in metric_keys}

    mst = algo_time_stats(
        np.array([raw["mst_kruskal"], raw["mst_prim"], raw["mst_boruvka"]], dtype=float),
        ["Краскал", "Прим", "Борувка"],
        np.asarray(raw["mst_weight"], dtype=float),
    )
    mst["agree_count"] = int(np.sum(np.asarray(raw["mst_agree"], dtype=bool)))

    apsp = algo_time_stats(
        np.array([raw["fw"], raw["dij_all"], raw["johnson"]], dtype=float),
        ["Флойд–Уоршелл", "Дейкстра × V", "Джонсон"],
        np.asarray(raw["avg_dij"], dtype=float),
    )
    apsp["agree_count"] = int(np.sum(np.asarray(raw["apsp_agree"], dtype=bool)))

    pair = algo_time_stats(
        np.array([raw["dij_pair"], raw["bidir"]], dtype=float),
        ["Дейкстра", "Двунаправленный Дейкстра"],
        np.asarray(raw["path_len"], dtype=float),
    )
    pair["agree_count"] = int(np.sum(np.asarray(raw["pair_agree"], dtype=bool)))

    flow = algo_time_stats(
        np.array([raw["ek"], raw["dinic"], raw["pr"]], dtype=float),
        ["Эдмондс–Карп", "Диниц", "Push–Relabel"],
        np.asarray(raw["flow_value"], dtype=float),
    )
    flow["agree_count"] = int(np.sum(np.asarray(raw["flow_agree"], dtype=bool)))

    return {
        "probs": list(raw["probs"]),
        "graph_stats": graph_stats,
        "mst": mst,
        "apsp": apsp,
        "pair": pair,
        "flow": flow,
        "V": V,
        "n": len(raw["edges"]),
    }


def run_one(
    mu: float,
    b: float,
    p: float,
    V: int = TZ_V,
    num_graphs: int = TZ_NUM_GRAPHS,
    seed: int = 42,
    preset_name: str | None = None,
) -> dict[str, Any]:
    raw = _run_cpp({
        "mode": "experiment",
        "mu": mu,
        "b": b,
        "p": p,
        "V": V,
        "num_graphs": num_graphs,
        "base_seed": seed,
    })
    summary = summarize_raw(raw, V)
    summary["params"] = {
        "mu": mu,
        "b": b,
        "p": p,
        "V": V,
        "num_graphs": num_graphs,
        "preset_name": preset_name or f"μ={mu}, b={b}",
    }
    return summary


def run_batch(
    mu: float,
    b: float,
    p_mode: str,
    V: int = TZ_V,
    num_graphs: int = TZ_NUM_GRAPHS,
    seed: int = 42,
    preset_name: str | None = None,
) -> list[dict[str, Any]]:
    """Один набор (μ,b) × одно p или все p."""
    ps = P_VALUES if p_mode == "all" else [float(p_mode)]
    return [run_one(mu, b, p, V, num_graphs, seed, preset_name) for p in ps]


def run_full_tz(
    V: int = TZ_V,
    num_graphs: int = TZ_NUM_GRAPHS,
    seed: int = 42,
) -> list[dict[str, Any]]:

    if V < TZ_V:
        raise ValueError(f"По ТЗ нужно V >= {TZ_V}, получено V={V}")
    if num_graphs < TZ_NUM_GRAPHS:
        raise ValueError(f"По ТЗ нужно >= {TZ_NUM_GRAPHS} графов в выборке, получено {num_graphs}")

    results: list[dict[str, Any]] = []
    for pr in PRESETS:
        for p in P_VALUES:
            results.append(
                run_one(pr["mu"], pr["b"], p, V, num_graphs, seed, preset_name=pr["name"])
            )
    return results


def build_property_charts(results: list[dict[str, Any]]) -> dict[str, Any]:
    """
    Данные для графиков влияния p и параметров распределения на свойства графов
    Группировка: линия = пресет (μ,b), ось X = p.
    """
    # ключ пресета → {p → mean metric}
    metrics = ["density", "num_components", "deg_avg", "avg_shortest_path", "weighted_diameter", "w_avg"]
    titles = {
        "density": "Плотность",
        "num_components": "Число компонент",
        "deg_avg": "Средняя степень",
        "avg_shortest_path": "Средняя длина кратчайшего пути",
        "weighted_diameter": "Взвешенный диаметр",
        "w_avg": "Средний вес ребра",
    }

    by_preset: dict[str, dict[float, dict[str, float]]] = {}
    for res in results:
        name = res["params"].get("preset_name") or f"μ={res['params']['mu']}"
        p = float(res["params"]["p"])
        by_preset.setdefault(name, {})
        by_preset[name][p] = {
            m: res["graph_stats"][m]["mean"] for m in metrics
        }

    # Chart.js datasets: для каждой метрики — список линий по пресетам
    charts: dict[str, Any] = {"p_labels": P_VALUES, "titles": titles, "series": {}}
    for m in metrics:
        series = []
        for name, pdata in by_preset.items():
            series.append({
                "label": name,
                "data": [pdata.get(p, {}).get(m) for p in P_VALUES],
            })
        charts["series"][m] = series


    charts["insights"] = _insights(results)
    return charts


def _insights(results: list[dict[str, Any]]) -> list[str]:
    """Краткие выводы по структуре (вопросы ТЗ 1–5 о свойствах)."""
    if not results:
        return []
    lines: list[str] = []

    # Плотность vs p (усреднение по пресетам)
    dens_by_p: dict[float, list[float]] = {p: [] for p in P_VALUES}
    comp_by_p: dict[float, list[float]] = {p: [] for p in P_VALUES}
    for res in results:
        p = float(res["params"]["p"])
        if p in dens_by_p:
            dens_by_p[p].append(res["graph_stats"]["density"]["mean"])
            comp_by_p[p].append(res["graph_stats"]["num_components"]["mean"])

    dens_means = {p: float(np.mean(v)) if v else 0.0 for p, v in dens_by_p.items()}
    if dens_means.get(0.4, 0) > dens_means.get(0.05, 0):
        lines.append(
            f"1. Плотность растёт с p: "
            f"{dens_means.get(0.05, 0):.4f} (p=0.05) -> {dens_means.get(0.4, 0):.4f} (p=0.4)."
        )
    else:
        lines.append("1. Ожидалось рост плотности с p — проверьте выборку.")

    comp_means = {p: float(np.mean(v)) if v else 0.0 for p, v in comp_by_p.items()}
    lines.append(
        f"2. Компоненты связности: среднее {comp_means.get(0.05, 0):.2f} при p=0.05 "
        f"и {comp_means.get(0.4, 0):.2f} при p=0.4 (обычно падает при росте p)."
    )

    # Веса при разных параметрах
    for pr in PRESETS:
        subset = [r for r in results if r["params"].get("preset_name") == pr["name"]]
        if not subset:
            continue
        wavg = float(np.mean([r["graph_stats"]["w_avg"]["mean"] for r in subset]))
        lines.append(f"4. Пресет «{pr['name']}» (μ={pr['mu']}, b={pr['b']}): средний вес ребра ≈ {wavg:.2f}.")

    # ASP / диаметр vs параметры
    for pr in PRESETS:
        subset = [r for r in results if r["params"].get("preset_name") == pr["name"]]
        if not subset:
            continue
        asp = float(np.mean([r["graph_stats"]["avg_shortest_path"]["mean"] for r in subset]))
        dia = float(np.mean([r["graph_stats"]["weighted_diameter"]["mean"] for r in subset]))
        lines.append(
            f"5. «{pr['name']}»: средняя длина КПут. ≈ {asp:.2f}, взвеш. диаметр ≈ {dia:.2f}."
        )

    # Степени
    deg_by_p: dict[float, list[float]] = {p: [] for p in P_VALUES}
    for res in results:
        p = float(res["params"]["p"])
        if p in deg_by_p:
            deg_by_p[p].append(res["graph_stats"]["deg_avg"]["mean"])
    deg_means = {p: float(np.mean(v)) if v else 0.0 for p, v in deg_by_p.items()}
    lines.insert(
        2,
        f"3. Средняя степень растёт с p: "
        f"{deg_means.get(0.05, 0):.2f} -> {deg_means.get(0.4, 0):.2f}.",
    )
    return lines


def probs_only(mu: float, b: float) -> list[float]:
    raw = _run_cpp({"mode": "probs", "mu": mu, "b": b})
    return list(raw["probs"])


def all_presets_probs() -> list[dict[str, Any]]:
    """P(w) для каждого пресета — для таблицы/графика в отчёте."""
    out = []
    for pr in PRESETS:
        out.append({
            "name": pr["name"],
            "mu": pr["mu"],
            "b": pr["b"],
            "probs": probs_only(pr["mu"], pr["b"]),
        })
    return out

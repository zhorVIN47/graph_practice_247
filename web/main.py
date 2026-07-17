"""
Веб-интерфейс Flask без Jinja2.
UI: static/index.html + static/app.js
API: JSON endpoints; вычисления через experiment.py -> graph_core.exe
"""
import os
import sys

from flask import Flask, jsonify, request, send_from_directory

sys.path.insert(0, os.path.dirname(__file__))
from experiment import (
    PRESETS,
    P_VALUES,
    TZ_NUM_GRAPHS,
    TZ_V,
    all_presets_probs,
    build_property_charts,
    core_available,
    probs_only,
    run_batch,
)

STATIC_DIR = os.path.join(os.path.dirname(__file__), "static")
app = Flask(__name__, static_folder=STATIC_DIR, static_url_path="/static")


@app.get("/")
def index():
    return send_from_directory(STATIC_DIR, "index.html")


@app.get("/api/meta")
def api_meta():
    ok, err = core_available()
    return jsonify({
        "presets": PRESETS,
        "p_values": P_VALUES,
        "tz_v": TZ_V,
        "tz_n": TZ_NUM_GRAPHS,
        "core_ok": ok,
        "core_err": err,
    })


def _parse_run_body():
    data = request.get_json(force=True, silent=True) or {}
    preset = str(data.get("preset", "custom"))
    preset_name = None
    if preset != "custom" and preset.isdigit() and int(preset) < len(PRESETS):
        pr = PRESETS[int(preset)]
        mu, b = float(pr["mu"]), float(pr["b"])
        preset_name = pr["name"]
    else:
        mu = float(data.get("mu", 25.0))
        b = float(data.get("b", 5.0))

    p_mode = str(data.get("p_mode", "all"))
    V = int(data.get("V", TZ_V))
    num_graphs = int(data.get("num_graphs", TZ_NUM_GRAPHS))
    return mu, b, p_mode, V, num_graphs, preset_name, preset


@app.post("/api/run")
def api_run():
    ok, err = core_available()
    if not ok:
        return jsonify({"error": err}), 500
    mu, b, p_mode, V, num_graphs, preset_name, _ = _parse_run_body()
    if V < 2:
        return jsonify({"error": "V >= 2"}), 400
    if num_graphs < 1:
        return jsonify({"error": "num_graphs >= 1"}), 400

    batch = run_batch(mu, b, p_mode, V, num_graphs, preset_name=preset_name)
    charts = build_property_charts(batch) if len(batch) >= 2 else None
    return jsonify({
        "mode": "single",
        "results": batch,
        "probs_preview": probs_only(mu, b),
        "property_charts": charts,
    })


@app.post("/api/probs")
def api_probs():
    ok, err = core_available()
    if not ok:
        return jsonify({"error": err}), 500
    data = request.get_json(force=True, silent=True) or {}
    if data.get("all_presets"):
        return jsonify({"mode": "probs_all", "all_probs": all_presets_probs()})

    preset = str(data.get("preset", "custom"))
    if preset != "custom" and preset.isdigit() and int(preset) < len(PRESETS):
        mu = float(PRESETS[int(preset)]["mu"])
        b = float(PRESETS[int(preset)]["b"])
    else:
        mu = float(data.get("mu", 25.0))
        b = float(data.get("b", 5.0))
    return jsonify({"mode": "probs", "probs_preview": probs_only(mu, b), "mu": mu, "b": b})


if __name__ == "__main__":
    # use_reloader=False — на Windows два процесса на :5000 ломают браузер
    # host=0.0.0.0 — доступ и с 127.0.0.1, и с localhost
    print("Запуск: http://127.0.0.1:5000  (или http://localhost:5000)")
    print("Остановка: Ctrl+C")
    app.run(
        host="0.0.0.0",
        port=5000,
        debug=False,
        use_reloader=False,
        threaded=False,
    )



const METRIC_LABELS = {
  edges: "Рёбра",
  density: "Плотность",
  num_components: "Компоненты",
  lcc_size: "Размер LCC",
  lcc_ratio: "Доля LCC",
  deg_min: "Степень min",
  deg_avg: "Степень avg",
  deg_max: "Степень max",
  w_min: "Вес min",
  w_avg: "Вес avg",
  w_median: "Вес median",
  w_max: "Вес max",
  avg_shortest_path: "Средняя длина КПут.",
  weighted_diameter: "Взвеш. диаметр",
};

const COLORS = ["#ef4444", "#22c55e", "#3d9cf0", "#eab308", "#a855f7"];
let META = null;
const charts = [];

function fmt(x, digits = 4) {
  if (x === null || x === undefined || Number.isNaN(x)) return "-";
  const n = Number(x);
  if (Math.abs(n) >= 1000 || (Math.abs(n) > 0 && Math.abs(n) < 0.001)) return n.toExponential(3);
  return Number(n.toPrecision(digits)).toString();
}

function formPayload() {
  return {
    preset: document.getElementById("preset").value,
    mu: parseFloat(document.getElementById("mu").value),
    b: parseFloat(document.getElementById("b").value),
    p_mode: document.getElementById("p_mode").value,
    V: parseInt(document.getElementById("V").value, 10),
    num_graphs: parseInt(document.getElementById("num_graphs").value, 10),
  };
}

function setBusy(busy, msg) {
  ["btnRun", "btnProbs", "btnProbsAll"].forEach((id) => {
    document.getElementById(id).disabled = busy;
  });
  const st = document.getElementById("status");
  if (busy) {
    st.hidden = false;
    st.textContent = msg || "Считаем… (C++ может работать долго)";
  } else {
    st.hidden = true;
  }
}

async function api(path, body) {
  const res = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body || {}),
  });
  const text = await res.text();
  let data;
  try {
    data = JSON.parse(text);
  } catch (_) {
    throw new Error(
      "Сервер вернул не JSON (часто HTML-ошибка Flask). Смотри терминал с main.py.\n" +
      text.slice(0, 200)
    );
  }
  if (!res.ok) throw new Error(data.error || res.statusText);
  return data;
}

function destroyCharts() {
  while (charts.length) {
    const c = charts.pop();
    try { c.destroy(); } catch (_) {}
  }
}

function makeChart(canvas, cfg) {
  const c = new Chart(canvas, cfg);
  charts.push(c);
  return c;
}

function algoTable(block, n) {
  let rows = "";
  for (let i = 0; i < block.names.length; i++) {
    const name = block.names[i];
    rows += `<tr>
      <td><b>${name}</b></td>
      <td>${fmt(block.means[i], 5)}</td>
      <td>${fmt(block.medians[i], 5)}</td>
      <td>${fmt(block.mins[i], 5)}</td>
      <td>${fmt(block.maxs[i], 5)}</td>
      <td>${block.wins[name]}</td>
    </tr>`;
  }
  const extra = block.result_mean !== undefined
    ? ` · ср. результат: ${fmt(block.result_mean)}`
    : "";
  return `
    <table>
      <tr><th>Алгоритм</th><th>Среднее</th><th>Медиана</th><th>Мин</th><th>Макс</th><th>Побед</th></tr>
      ${rows}
    </table>
    <p class="meta">Совпадений результата: <span class="ok">${block.agree_count}/${n}</span>${extra}</p>
  `;
}

function probsTable(probs) {
  const heads = Array.from({ length: 50 }, (_, i) => `<th>${i + 1}</th>`).join("");
  const vals = probs.map((p) => `<td>${Number(p).toFixed(4)}</td>`).join("");
  return `<div class="scroll-x"><table><tr><th>k</th>${heads}</tr><tr><td>P</td>${vals}</tr></table></div>`;
}

function renderProbsBar(canvas, probs, label) {
  makeChart(canvas, {
    type: "bar",
    data: {
      labels: Array.from({ length: 50 }, (_, i) => i + 1),
      datasets: [{ label: label || "P(w=k)", data: probs, backgroundColor: "#2dd4bf88" }],
    },
    options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false } } },
  });
}

function renderPropertyCharts(pc) {
  if (!pc || !pc.series) return "";
  let html = `<div class="panel"><h2>Влияние p и параметров на свойства графов</h2>`;
  if (pc.insights && pc.insights.length) {
    html += `<h3>Выводы по структуре</h3><ul class="insights">`;
    pc.insights.forEach((line) => { html += `<li>${line}</li>`; });
    html += `</ul>`;
  }
  Object.keys(pc.titles || {}).forEach((key) => {
    html += `<h3>${pc.titles[key]}</h3><div class="chart-box"><canvas id="prop_${key}"></canvas></div>`;
  });
  html += `</div>`;
  return html;
}

function mountPropertyCharts(pc) {
  if (!pc || !pc.series) return;
  Object.keys(pc.series).forEach((key) => {
    const el = document.getElementById("prop_" + key);
    if (!el) return;
    makeChart(el, {
      type: "line",
      data: {
        labels: pc.p_labels,
        datasets: pc.series[key].map((s, i) => ({
          label: s.label,
          data: s.data,
          borderColor: COLORS[i % COLORS.length],
          backgroundColor: COLORS[i % COLORS.length] + "33",
          tension: 0.2,
        })),
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        scales: { y: { beginAtZero: true } },
      },
    });
  });
}

function renderResults(data) {
  destroyCharts();
  const out = document.getElementById("out");
  let html = "";

  if (data.mode === "full_tz") {
    html += `<div class="panel"><p class="ok">Полный прогон по ТЗ: ${(data.results || []).length} выборок.</p></div>`;
  }

  if (data.all_probs) {
    html += `<div class="panel"><h2>P(w) для всех наборов параметров</h2>`;
    data.all_probs.forEach((item, idx) => {
      html += `<h3>${item.name} · μ=${item.mu}, b=${item.b}</h3>`;
      html += `<div class="chart-box"><canvas id="allProbs${idx}"></canvas></div>`;
      html += probsTable(item.probs);
    });
    html += `</div>`;
  }

  if (data.probs_preview) {
    html += `<div class="panel"><h2>Таблица вероятностей P(w = k)</h2>`;
    html += `<div class="chart-box"><canvas id="probsOnly"></canvas></div>`;
    html += probsTable(data.probs_preview);
    html += `</div>`;
  }

  if (data.property_charts) {
    html += renderPropertyCharts(data.property_charts);
  }

  (data.results || []).forEach((res, idx) => {
    const p = res.params;
    html += `<div class="panel">
      <h2>Выборка · ${p.preset_name || ""} · μ=${p.mu}, b=${p.b}, p=${p.p}, V=${p.V}, n=${res.n}</h2>
      <h3>1. Распределение весов P(w)</h3>
      <div class="chart-box"><canvas id="probsChart${idx}"></canvas></div>
      <h3>2. Характеристики графов</h3>
      <table><tr><th>Показатель</th><th>Среднее</th><th>Медиана</th><th>Мин</th><th>Макс</th></tr>`;
    Object.keys(METRIC_LABELS).forEach((key) => {
      const s = res.graph_stats[key];
      html += `<tr><td>${METRIC_LABELS[key]}</td><td>${fmt(s.mean)}</td><td>${fmt(s.median)}</td><td>${fmt(s.min)}</td><td>${fmt(s.max)}</td></tr>`;
    });
    html += `</table>
      <h3>3. Сравнение алгоритмов (мс)</h3>
      <p><b>Задача 1 — MST</b></p>${algoTable(res.mst, res.n)}
      <p><b>Задача 2 — APSP</b> (совпадение матриц расстояний)</p>${algoTable(res.apsp, res.n)}
      <p><b>Задача 3 — путь</b> (BFS farthest pair)</p>${algoTable(res.pair, res.n)}
      <p><b>Задача 4 — max-flow</b> (s,t по взвешенному расстоянию)</p>${algoTable(res.flow, res.n)}
      <h3>4. Среднее время</h3>
      <div class="chart-box"><canvas id="timeChart${idx}"></canvas></div>
    </div>`;
  });

  out.innerHTML = html;

  if (data.all_probs) {
    data.all_probs.forEach((item, idx) => {
      renderProbsBar(document.getElementById("allProbs" + idx), item.probs, item.name);
    });
  }
  if (data.probs_preview) {
    renderProbsBar(document.getElementById("probsOnly"), data.probs_preview);
  }
  mountPropertyCharts(data.property_charts);

  (data.results || []).forEach((res, idx) => {
    renderProbsBar(document.getElementById("probsChart" + idx), res.probs);
    makeChart(document.getElementById("timeChart" + idx), {
      type: "bar",
      data: {
        labels: ["MST", "APSP", "Пара", "Поток"],
        datasets: [
          { label: "Алг. 1", data: [res.mst.means[0], res.apsp.means[0], res.pair.means[0], res.flow.means[0]], backgroundColor: "#ef4444" },
          { label: "Алг. 2", data: [res.mst.means[1], res.apsp.means[1], res.pair.means[1], res.flow.means[1]], backgroundColor: "#22c55e" },
          { label: "Алг. 3", data: [res.mst.means[2], res.apsp.means[2], 0, res.flow.means[2]], backgroundColor: "#eab308" },
        ],
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: { title: { display: true, text: "Среднее время (мс), p=" + res.params.p } },
        scales: { y: { beginAtZero: true } },
      },
    });
  });
}

async function init() {
  const res = await fetch("/api/meta");
  META = await res.json();

  document.getElementById("badge").textContent =
    `Вариант 7 · Laplace · V≥${META.tz_v}, n=${META.tz_n}`;

  const preset = document.getElementById("preset");
  preset.innerHTML = `<option value="custom">Свои μ, b</option>` +
    META.presets.map((pr, i) =>
      `<option value="${i}">${pr.name} (μ=${pr.mu}, b=${pr.b})</option>`
    ).join("");
  preset.value = "0";

  const pMode = document.getElementById("p_mode");
  pMode.innerHTML = META.p_values.map((p) => `<option value="${p}">p = ${p}</option>`).join("") +
    `<option value="all" selected>все p (0.05, 0.15, 0.4)</option>`;

  document.getElementById("V").value = META.tz_v;
  document.getElementById("num_graphs").value = META.tz_n;
  document.getElementById("mu").value = META.presets[0].mu;
  document.getElementById("b").value = META.presets[0].b;

  if (!META.core_ok) {
    const w = document.getElementById("coreWarn");
    w.hidden = false;
    w.innerHTML = `C++ ядро graph_core.exe не найдено: ${META.core_err}<br>Сборка: <code>powershell -File cpp\\build.ps1</code>`;
  }

  preset.addEventListener("change", () => {
    if (preset.value !== "custom" && META.presets[preset.value]) {
      document.getElementById("mu").value = META.presets[preset.value].mu;
      document.getElementById("b").value = META.presets[preset.value].b;
    }
  });

  document.getElementById("mainForm").addEventListener("submit", async (e) => {
    e.preventDefault();
    setBusy(true, "Запуск выборки…");
    try {
      renderResults(await api("/api/run", formPayload()));
    } catch (err) {
      alert(err.message);
    } finally {
      setBusy(false);
    }
  });

  document.getElementById("btnProbs").addEventListener("click", async () => {
    setBusy(true, "Считаем P(w)…");
    try {
      renderResults(await api("/api/probs", formPayload()));
    } catch (err) {
      alert(err.message);
    } finally {
      setBusy(false);
    }
  });

  document.getElementById("btnProbsAll").addEventListener("click", async () => {
    setBusy(true, "Считаем P(w) для всех пресетов…");
    try {
      renderResults(await api("/api/probs", { all_presets: true }));
    } catch (err) {
      alert(err.message);
    } finally {
      setBusy(false);
    }
  });
}

init().catch((e) => alert("Не удалось загрузить /api/meta: " + e.message));

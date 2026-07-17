# Учебная практика — вариант 7 (Лаплас)

## Архитектура
```
web/static/          — HTML/JS/CSS (без Jinja2)
web/main.py          — Flask JSON API
web/experiment.py    — оркестрация + NumPy
cpp/graph_core.exe   — вычисления (params.json ↔ result.json)
```

## Сборка C++
```powershell
cd c:\Users\kiril\practice_7var\cpp
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

## Запуск
```powershell
cd c:\Users\kiril\practice_7var
.\venv\Scripts\python.exe -m pip install -r web\requirements.txt
.\venv\Scripts\python.exe web\main.py
```
http://127.0.0.1:5000

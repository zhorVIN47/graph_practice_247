from setuptools import setup, Extension

setup(
    name="graph_core",
    ext_modules=[
        Extension(
            "graph_core",
            sources=["main.cpp", "algorithms.cpp"],
            include_dirs=["."],
            language="c++",
            extra_compile_args=["/std:c++17"],
        )
    ],
)
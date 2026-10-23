# Vendored GUI libraries

| Library | Version | Commit | License |
|---|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.93.1 WIP, docking branch | d88f238 (2026-10-15) | MIT |
| [ImPlot](https://github.com/epezent/implot) | 1.1 WIP | 851cdbd (2026-10-10) | MIT |
| [cimgui](https://github.com/cimgui/cimgui) | generator at 2a6a93f | regenerated, see below | MIT |
| [cimplot](https://github.com/cimgui/cimplot) | generator at 73312c0 | regenerated, see below | MIT |

The fonts in imgui/fonts are Roboto Medium and Cousine (Apache License 2.0),
as distributed with Dear ImGui.

## Regenerating the C bindings

The published cimgui files wrap mainline ImGui, so they lack the docking
API. The bindings here were regenerated from the docking branch:

    git clone -b docking https://github.com/ocornut/imgui
    git clone https://github.com/cimgui/cimgui && rm -rf cimgui/imgui && cp -r imgui cimgui/imgui
    (cd cimgui/generator && luajit ./generator.lua gcc "internal noimstrv")
    git clone --recursive https://github.com/cimgui/cimplot
    (cd cimplot/generator && luajit ./generator.lua gcc "internal")

Then copy cimgui.{h,cpp} and cimplot.{h,cpp}. Backend glue is in
app/src/gui_backend.cpp and needs no generated bindings.

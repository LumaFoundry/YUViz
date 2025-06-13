@echo off
qsb --glsl 100es,120,150 --hlsl 50 --msl 12 -o src/shaders/vertex.qsb src/shaders/vertex.vert
qsb --glsl 100es,120,150 --hlsl 50 --msl 12 -o src/shaders/fragment.qsb src/shaders/fragment.frag 
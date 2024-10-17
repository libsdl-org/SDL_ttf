#!/usr/bin/sh

glslc -o ./bin/shader_spv.vert ./src/shader_glsl.vert
glslc -o ./bin/shader_spv.frag ./src/shader_glsl.frag

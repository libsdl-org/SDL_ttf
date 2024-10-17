#version 450

layout (location = 0) in vec3 vpos;
layout (location = 1) in vec4 vcolour;
layout (location = 2) in vec2 tex_coord;

layout (location = 0) out vec4 colour;
layout (location = 1) out vec2 coord;

layout (set = 1, binding = 0) uniform uniform_block {
    mat4 proj_view;
    mat4 model;
};

void main() {
    colour = vcolour;
    coord = tex_coord;
    gl_Position = proj_view * model * vec4(vpos, 1.0f);
}

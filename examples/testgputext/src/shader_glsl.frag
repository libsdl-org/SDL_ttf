#version 450

layout (location = 0) in vec4 vcolour;
layout (location = 1) in vec2 tex_coord;

layout (location = 0) out vec4 frag_colour;

layout (set = 2, binding = 0) uniform sampler2D tex;


void main() {
	frag_colour = vcolour * texture(tex, tex_coord);
}

#version 450
layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_uv;

void main() {
	vec2 p      = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	v_uv        = p;
	v_color     = vec4(1);
	gl_Position = vec4(p * 2 - 1, 0, 1);
}

#version 450
// ScaleFX by Sp00kyFox, MIT; see scale_scalefx.h for the full license.
// Vulkan fragment shaders for SDL 3.4's GPU renderer. Locations match SDL's vertex shader.
layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 0) uniform sampler2D Source;
layout(set = 2, binding = 1) uniform sampler2D Auxiliary;

layout(std140, set = 3, binding = 0) uniform Params {
	ivec4 settings;
};

vec4 readTex(sampler2D image, ivec2 offset) {
	return texelFetch(image, clamp(ivec2(gl_FragCoord.xy) + offset, ivec2(0), textureSize(image, 0) - 1), 0);
}

#define SFX_CLR     0.5
#define SFX_SAA     1.0
#define SFX_SCN     1.0
#define PassOutput0 Auxiliary

// Palette distances are computed once on the CPU when RGB values change.
// This avoids vendor-dependent sqrt rounding in edge/junction comparisons.
float metric(int a, int b) {
	return texelFetch(Auxiliary, ivec2(b, a), 0).r;
}

int indexAt(ivec2 offset) {
	return int(round(readTex(Source, offset).a * 255.));
}

void main() {
	int e = indexAt(ivec2(0));
	FragColor
			= vec4(metric(e, indexAt(ivec2(-1, -1))), metric(e, indexAt(ivec2(0, -1))), metric(e, indexAt(ivec2(1, -1))),
				   metric(e, indexAt(ivec2(1, 0))));
}

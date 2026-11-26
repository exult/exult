#version 450
// ScaleFX by Sp00kyFox, MIT; see scale_scalefx.h for the full license.
// Vulkan fragment shaders for SDL 3.4's GPU renderer. Locations match SDL's vertex shader.
layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 0) uniform sampler2D Source;

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

// corner strength
float str(float d, vec2 a, vec2 b) {
	float diff  = a.x - a.y;
	float wght1 = max(SFX_CLR - d, 0.) / SFX_CLR;
	float wght2 = clamp((1. - d) + (min(a.x, b.x) + a.x > min(a.y, b.y) + a.y ? diff : -diff), 0., 1.);
	return (SFX_SAA == 1. || 2. * d < a.x + a.y) ? (wght1 * wght2) * (a.x * a.y) : 0.;
}

void main() {
	/*	grid		metric		pattern

		A B		x y z		x y
		D E F		  o w		w z
		G H I
	*/

#define TEX(x, y) readTex(Source, ivec2(x, y))

	// metric data
	vec4 A = TEX(-1, -1), B = TEX(0, -1);
	vec4 D = TEX(-1, 0), E = TEX(0, 0), F = TEX(1, 0);
	vec4 G = TEX(-1, 1), H = TEX(0, 1), I = TEX(1, 1);

	// corner strength
	vec4 res;
	res.x = str(D.z, vec2(D.w, E.y), vec2(A.w, D.y));
	res.y = str(F.x, vec2(E.w, E.y), vec2(B.w, F.y));
	res.z = str(H.z, vec2(E.w, H.y), vec2(H.w, I.y));
	res.w = str(H.x, vec2(D.w, H.y), vec2(G.w, G.y));

	FragColor = res;
}

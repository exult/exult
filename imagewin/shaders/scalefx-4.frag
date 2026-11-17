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

const ivec2 offsets[9] = ivec2[9](
		ivec2(0, 0), ivec2(-1, 0), ivec2(-2, 0), ivec2(1, 0), ivec2(2, 0), ivec2(0, -1), ivec2(0, -2), ivec2(0, 1), ivec2(0, 2));

vec4 original(ivec2 p, int code) {
	return texelFetch(Auxiliary, clamp(p + offsets[code], ivec2(0), settings.xy - 1), 0);
}

void main() {
	int   f     = settings.z;
	ivec2 coord = ivec2(gl_FragCoord.xy), p = coord / f, sub = coord % f;
	vec4  encoded = texelFetch(Source, p, 0), centre = original(p, 0), colour = centre;
	if (f == 3) {
		ivec4 corner = ivec4(encoded) % 9, mid = ivec4(encoded) / 9;
		int   code;
		if (sub.y == 0) {
			code = sub.x == 0 ? corner.x : sub.x == 1 ? mid.x : corner.y;
		} else if (sub.y == 1) {
			code = sub.x == 0 ? mid.w : sub.x == 1 ? 0 : mid.y;
		} else {
			code = sub.x == 0 ? corner.w : sub.x == 1 ? mid.z : corner.z;
		}
		colour = original(p, code);
	} else {
		int  best = -1, denom = 1;
		bool conflict = false;
		for (int c = 0; c < 4; ++c) {
			int value = int(encoded[c]);
			int code  = value % 9;
			if (code == 0) {
				continue;
			}
			int nx = (value / 9) % 7, ny = (value / 63) % 7, t = value / 441;
			int u = (c == 1 || c == 2) ? f - 1 - sub.x : sub.x, v = c >= 2 ? f - 1 - sub.y : sub.y;
			int depth = f * t - 6 * (nx * (2 * u + 1) + ny * (2 * v + 1)), norm = nx + ny;
			if (depth < 0) {
				continue;
			}
			vec4 candidate = original(p, code);
			int  index = int(round(candidate.a * 255.)), centre_index = int(round(centre.a * 255.));
			if (depth == 0 && index > centre_index) {
				continue;
			}
			if (depth * denom > best * norm) {
				best     = depth;
				denom    = norm;
				colour   = candidate;
				conflict = false;
			} else if (depth * denom == best * norm && candidate.a != colour.a) {
				conflict = true;
			}
		}
		if (conflict) {
			colour = centre;
		}
	}
	FragColor = vec4(colour.rgb, 1.);
}

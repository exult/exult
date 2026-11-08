#version 450
layout(location=0) in vec4 v_color;
layout(location=1) in vec2 v_uv;
layout(location=0) out vec4 FragColor;
layout(set=2,binding=0) uniform usampler2D Pattern0;
layout(set=2,binding=1) uniform usampler2D Pattern1;
layout(set=2,binding=2) uniform usampler2D Pattern2;
layout(set=2,binding=3) uniform usampler2D Pattern3;
layout(std140,set=3,binding=0) uniform Params { ivec4 settings; };
void main() {
 ivec2 coord=ivec2(gl_FragCoord.xy), p=coord/settings.z,sub=coord%settings.z;
 int index=sub.y*settings.z+sub.x;
 uvec4 block;
 if(index<4) block=texelFetch(Pattern0,p,0);
 else if(index<8) block=texelFetch(Pattern1,p,0);
 else if(index<12) block=texelFetch(Pattern2,p,0);
 else block=texelFetch(Pattern3,p,0);
 uint value=block[index%4];
 FragColor=vec4(float((value>>16)&255u),float((value>>8)&255u),float(value&255u),255.)/255.;
}

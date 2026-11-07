#version 450
layout(location=0) out vec4 FragColor;
layout(set=2,binding=0) uniform sampler2D A;
layout(set=2,binding=1) uniform sampler2D B;
layout(std140,set=3,binding=0) uniform Params { ivec4 mode; ivec4 bounds; };
void main() {
    ivec2 start=ivec2(gl_FragCoord.xy)*2;
    ivec2 size=textureSize(A,0);
    float flag=0.;
    for(int y=0;y<2;++y) for(int x=0;x<2;++x) {
        ivec2 p=start+ivec2(x,y);
        if(any(greaterThanEqual(p,size))) continue;
        if(mode.x==0) {
            if(any(lessThan(p,bounds.xy)) || any(greaterThanEqual(p,bounds.xy+bounds.zw))) continue;
            ivec3 a=ivec3(round(texelFetch(A,p,0).rgb*255.));
            ivec3 b=ivec3(round(texelFetch(B,p,0).rgb*255.));
            if(abs((a.r-b.r)-(b.g-a.g))>96) flag=1.;
        } else flag=max(flag,texelFetch(A,p,0).r);
    }
    FragColor=vec4(flag,0,0,1);
}

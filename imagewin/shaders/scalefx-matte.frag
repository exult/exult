#version 450
layout(location=0) out vec4 FragColor;
layout(set=2,binding=0) uniform sampler2D Red;
layout(set=2,binding=1) uniform sampler2D Green;
layout(set=2,binding=2) uniform sampler2D Blue;
layout(set=2,binding=3) uniform sampler2D Indexed;
layout(set=2,binding=4) uniform sampler2D Palette;
layout(set=2,binding=5) uniform sampler2D Flag;
layout(std140,set=3,binding=0) uniform Params { ivec4 settings; };
void main() {
    ivec2 p=ivec2(gl_FragCoord.xy);
    int index=int(round(texelFetch(Indexed,p/settings.z,0).a*255.));
    ivec4 original=ivec4(round(texelFetch(Palette,ivec2(index,0),0)*255.));
    ivec3 a=ivec3(round(texelFetch(Red,p,0).rgb*255.));
    ivec3 b=ivec3(round(texelFetch(Green,p,0).rgb*255.));
    int ta=a.r-b.r, tb=b.g-a.g, sum=ta+tb, best=abs(ta-tb), use=1;
    if(texelFetch(Flag,ivec2(0),0).r>0.) {
        ivec3 c=ivec3(round(texelFetch(Blue,p,0).rgb*255.));
        int t13a=a.r-c.r, t13b=c.b-a.b, t23a=b.g-c.g, t23b=c.b-b.b;
        int s13=abs(t13a-t13b), s23=abs(t23a-t23b);
        if(s13<best) { best=s13; sum=t13a+t13b; use=1; }
        if(s23<best) { best=s23; sum=t23a+t23b; use=2; }
    }
    int coverage; ivec3 rgb;
    if(best>96) { coverage=index==settings.w ? 0:255; rgb=original.rgb; }
    else {
        coverage=255-clamp((sum+1)/2,0,255);
        if(coverage<=8) { coverage=0; rgb=ivec3(0); }
        else {
            ivec3 colour=use==1?a:b;
            colour[use==1?0:1]-=255-coverage;
            rgb=clamp(colour*255/coverage,ivec3(0),ivec3(255));
        }
    }
    int intrinsic=index==settings.w ? 255:original.a;
    FragColor=vec4(rgb,coverage*intrinsic/255)/255.;
}

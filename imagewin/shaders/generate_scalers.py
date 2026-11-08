#!/usr/bin/env python3
"""Translate Exult's existing scaler decision tables to the GPU (development only)."""
from pathlib import Path
import re
import os
import struct
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parent
SOURCE = ROOT.parent

def clean(s):
    return re.sub(r'/\*.*?\*/|//[^\n]*', '', s, flags=re.S)

def block(s, start):
    a = s.index('{', start)
    depth = 1
    b = a + 1
    while depth:
        depth += (s[b] == '{') - (s[b] == '}')
        b += 1
    return s[a+1:b-1]

COMMON = '''#version 450
// Generated from Exult's CPU scalers by generate_scalers.py; do not edit.
// HQx: MaxSt, LGPL-2.0-or-later. 2xSaI: Derek Liauw Kie Fa, LGPL-2.0-or-later.
// xBR: Hyllian, Zenju and Marzo Sette Torres Junior, GPL-2.0-or-later.
layout(location=0) out uvec4 Pattern0;
layout(location=1) out uvec4 Pattern1;
layout(set=2,binding=0) uniform sampler2D Source;
layout(set=2,binding=1) uniform sampler2D Palette;
layout(set=2,binding=2) uniform usampler2D Lookup;
layout(std140,set=3,binding=0) uniform Params { ivec4 settings; ivec4 limits; ivec4 output_group; };
uint pixels[16];
ivec2 position;
ivec4 fetchPixel(ivec2 delta) {
    return ivec4(round(texelFetch(Source,clamp(position+delta,ivec2(0),ivec2(settings.x,limits.y)-1),0)*255.));
}
uint pack(ivec3 c) { return uint((c.r<<16)|(c.g<<8)|c.b); }
ivec3 unpack(uint c) {return ivec3((c>>16)&255u,(c>>8)&255u,c&255u);}
ivec3 rgb(int index) {return ivec3(round(texelFetch(Palette,ivec2(index,0),0).rgb*255.));}
int indexAt(int x,int y) {return fetchPixel(ivec2(x,y)).a;}
'''
HQ_COMMON = '''
bool Diff(int a,int b) {
 return abs((a&0xFF0000)-(b&0xFF0000))>0x300000 || abs((a&0xFF00)-(b&0xFF00))>0x700 || abs((a&255)-(b&255))>6;
}
int toYUV(int c) {
 uint r=uint(c>>16)&255u,g=uint(c>>8)&255u,b=uint(c)&255u;
 uint Y=(r+g+b)>>2, u=128u+((r-b)>>2),v=128u+((2u*g-r-b)>>3);
 return int((Y<<16)+(u<<8)+v);
}
int StoreRGB(int c) {return c&0xFFFFFF;}
int Interp1(int a,int b) {return StoreRGB((a*3+b)>>2);}
int Interp2(int a,int b,int c) {return StoreRGB((a*2+b+c)>>2);}
int weighted(int a,int b,int c,int wa,int wb,int wc,int shift) {
 int green=((a&0x00FF00)*wa+(b&0x00FF00)*wb+(c&0x00FF00)*wc)&(0xFF00<<shift);
 int rb=((a&0xFF00FF)*wa+(b&0xFF00FF)*wb+(c&0xFF00FF)*wc)&(0xFF00FF<<shift);
 return (green+rb)>>shift;
}
int Interp3(int a,int b) {return weighted(a,b,0,7,1,0,3);}
int Interp4(int a,int b,int c) {return weighted(a,b,c,2,7,7,4);}
int Interp5(int a,int b) {return StoreRGB((a+b)>>1);}
int Interp6(int a,int b,int c) {return weighted(a,b,c,5,2,1,3);}
int Interp7(int a,int b,int c) {return weighted(a,b,c,6,1,1,3);}
int Interp8(int a,int b) {return weighted(a,b,0,5,3,0,3);}
int Interp9(int a,int b,int c) {return weighted(a,b,c,2,3,3,3);}
int Interp10(int a,int b,int c) {return weighted(a,b,c,14,1,1,4);}
'''

def hq(f):
    text = clean((SOURCE/f'scale_hq{f}x.h').read_text())
    defines = re.findall(r'#\s*define\s+(PIXEL\w+)\s+([^\n]+)', text)
    macros = {}
    for name, expression in defines:
        m = re.fullmatch(r'(\w+)<PTYPES>\((.*?),\s*(c\[.*),\s*manip\);',expression.strip())
        assert m, expression
        function, target, args = m.groups()
        offset = target.replace('to','0').replace('dline_pixels',str(f))
        macros[name] = f'pixels[{eval(offset, {"__builtins__":{}})}]=uint({function}({args}));'
    switch = 'switch (pattern) {'+block(text,text.index('switch (pattern)'))+'}'
    switch = re.sub(r'PIXEL\w+',lambda m:macros[m[0]],switch)
    return f'''void hq{f}() {{
 int c[10],w[10],yuv[10];c[0]=w[0]=yuv[0]=0;
 int k=1;
 for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {{
   w[k]=indexAt(x,y); ivec3 colour=rgb(w[k]);colour.r&=~3;colour.g&=~3;
   c[k]=int(pack(colour));yuv[k]=toYUV(c[k]);++k;
 }}
 int pattern=0,flag=1;
 for(k=1;k<=9;++k) {{ if(k==5) continue;
   if(w[k]!=w[5] && Diff(yuv[5],yuv[k])) pattern|=flag;flag<<=1;
 }}
 {switch}
}}\n'''

SAI_COMMON = '''
uint Interpolate_2xSaI(int a,int b) {return pack((rgb(a)+rgb(b))>>1);}
uint OInterpolate_2xSaI(int a,int b,int c) {return pack((rgb(a)*6+rgb(b)+rgb(c))>>3);}
uint QInterpolate_2xSaI(int a,int b,int c,int d) {return pack((rgb(a)+rgb(b)+rgb(c)+rgb(d))>>2);}
int GetResult1(int A,int B,int C,int D) {
 int x=0,y=0; if(A==C) ++x;else if(B==C) ++y;if(A==D) ++x;else if(B==D) ++y;
 return (x<=1?1:0)-(y<=1?1:0);
}
int GetResult2(int A,int B,int C,int D) {return -GetResult1(A,B,C,D);}
'''

def sai(name):
    text=clean((SOURCE/'scale_2xSaI.h').read_text())
    function=block(text,text.index('void Scale_'+name))
    body=block(function,function.index('for (int x ='))
    # Keep the CPU algorithm's local origin and asymmetric boundary rules.
    body=re.sub(r'\bSource_pixel\b','int',body)
    body=re.sub(r'\bDest_pixel\b','uint',body)
    body=re.sub(r'<int(?:, uint, Manip_pixels)?>','',body)
    body=re.sub(r'manip.copy\((\w+),\s*(\w+)\);',r'\1=pack(rgb(\2));',body)
    body=body.replace(', manip)',')')
    # Source pointer offsets become 2D texel offsets (rows use a virtual pitch).
    def fetch(m):
        expr=m[1].replace('bP','0')
        return 'indexAt(('+expr+')%4096, ('+expr+')/4096)'
    # Avoid signed modulus for negative row offsets: evaluate x/y components independently.
    def read(m):
        expr=m[1].replace('bP','0')
        rowvars={'prev1_yoff','next1_yoff','next2_yoff','prevl1','nextl1','nextl2'}
        xv=expr; yv=expr
        for var in re.findall(r'\b[A-Za-z_]\w*\b',expr):
            if var in rowvars: xv=re.sub(r'\b'+var+r'\b','0',xv)
            else: yv=re.sub(r'\b'+var+r'\b','0',yv)
        return f'indexAt({xv},({yv})/4096)'
    body=re.sub(r'\*\((bP[^)]*)\)',read,body)
    body=re.sub(r'\*dP\s*=\s*(\w+);',r'pixels[0]=\1;',body)
    body=re.sub(r'\*\(dP \+ 1\)\s*=\s*(\w+);',r'pixels[1]=\1;',body)
    body=re.sub(r'\*\(dP \+ dline_pixels\)\s*=\s*(\w+);',r'pixels[2]=\1;',body)
    body=re.sub(r'\*\(dP \+ dline_pixels \+ 1\)\s*=\s*(\w+);',r'pixels[3]=\1;',body)
    body=re.sub(r'bP \+= 1;|dP \+= 2;|prev1_xoff = 1;','',body)
    prefix='''int x=position.x-limits.z,y=position.y-limits.w;
 int sline_pixels=4096;
 int prev1_yoff=position.y>0?4096:0;
 int next1_yoff=position.y+1<limits.y?4096:0;
 int next2_yoff=min(2,max(0,limits.y-1-position.y))*4096;
 int prev1_xoff=position.x>0?1:0,next1_xoff=position.x+1<settings.x?1:0;
 int next2_xoff=min(2,max(0,settings.x-1-position.x));
 int xbeforelast=settings.x-2-limits.z;
 int xbeforelast1=settings.x-1-limits.z,xbeforelast2=settings.x-2-limits.z;
 int ybeforelast1=limits.y-1-limits.w,ybeforelast2=limits.y-2-limits.w;
'''
    return f'void {name}() {{\n{prefix}\n{body}\n}}\n'

def variant4(text):
    return re.sub(r'#\s*if XBR_VARIANT == 4\n(.*?)#\s*(?:elif|else).*?#\s*endif',r'\1',text,flags=re.S)

def xbr():
    cpp=variant4(clean((SOURCE/'scale_xbr.h').read_text()))
    h=variant4(clean((SOURCE/'scale_xbr.h').read_text()))
    methods=['blendLineSteepAndShallow','blendLineShallow','blendLineSteep','blendLineDiagonal','blendCorner']
    parts=['''
ivec2 rotateOffset(ivec2 p,int rotation) {
 for(int i=0;i<rotation;++i) p=ivec2(p.y,-p.x);return p;
}
int outputIndex(ivec2 p,int rotation) {
 for(int i=0;i<rotation;++i) p=ivec2(p.y,settings.z-1-p.x);return p.y*settings.z+p.x;
}
void blend(ivec2 p,ivec3 colour,int n,int m,int rotation) {
 int idx=outputIndex(p,rotation);pixels[idx]=pack((colour*n+unpack(pixels[idx])*(m-n))/m);
}
void put(ivec2 p,ivec3 colour,int rotation) {pixels[outputIndex(p,rotation)]=pack(colour);}
int dist(ivec3 a,ivec3 b) {ivec3 d=abs((a-b)*ivec3(3,4,2));return d.r+d.g+d.b;}
bool equals(ivec3 a,ivec3 b) {return all(lessThanEqual(abs((a-b)*ivec3(3,4,2)),ivec3(18,32,8)));}
''']
    # Preserve the CPU's double-precision 2.2 comparison at exact rational ties
    # without requiring FP64 support in Metal or mobile Vulkan.
    words=[0]*15
    for k in range(460):
        if 2.2*(5*k)<=11*k: words[k//32]|=1<<(k%32)
    parts += ['const uint steepTies[15]=uint[15]('+','.join(hex(x)+'u' for x in words)+');\n',
              '''bool steep(int a,int b) {
 int left=11*a,right=5*b;
 if(left!=right) return left<right;
 int k=a/5;return (steepTies[k/32]&(1u<<uint(k%32)))!=0u;
}\n''']
    for f in (2,3,4):
        st=block(cpp,cpp.index('struct Scaler'+str(f)+'xBR {'))
        for method in methods:
            body=block(st,st.index('static void '+method+'('))
            body=re.sub(r'out.template get<(\d+), (\d+)>\(\).template blend<(\d+), (\d+)>\(col\);',
                        lambda m:f'blend(ivec2({m[2]},{m[1]}),col,{m[3]},{m[4]},rotation);',body)
            for assignment in re.findall(r'(?:out.template get<\d+, \d+>\(\)\s*=\s*)+col;',body):
                coords=re.findall(r'get<(\d+), (\d+)>',assignment)
                body=body.replace(assignment,''.join(f'put(ivec2({j},{i}),col,rotation);' for i,j in coords))
            parts += [f'void {method}{f}(ivec3 col,int rotation) {{\n{body}\n}}\n']
    for method in methods:
        parts += [f'void {method}(ivec3 col,int rotation) {{if(settings.z==2) {method}2(col,rotation); else if(settings.z==3) {method}3(col,rotation); else {method}4(col,rotation);}}\n']
    body=block(h,h.index('scalePixel('))
    body=body[body.index('const int    weight'):]
    body=body.replace('const Buffer_Type& px','ivec3 px')
    body=re.sub(r'(\w+)\.dist\((\w+)\)',r'dist(\1,\2)',body)
    body=re.sub(r'(\w+)\.equals\((\w+)\)',r'equals(\1,\2)',body)
    body=body.replace('detectDirectionWeight * hf < ei','18 * hf < 5 * ei')
    body=body.replace('detectSteepWeight * fg <= hc','steep(fg,hc)').replace('fg >= detectSteepWeight * hc','steep(hc,fg)')
    body=re.sub(r'Scaler::(\w+)\(out, px\)',r'\1(px,rotation)',body)
    body=re.sub(r'const double \w+\s*= [0-9.]+;','',body)
    names={'a1':(-1,-2),'b1':(0,-2),'c1':(1,-2),'a0':(-2,-1),'a':(-1,-1),'b':(0,-1),'c':(1,-1),'c4':(2,-1),
           'd0':(-2,0),'d':(-1,0),'e':(0,0),'f':(1,0),'f4':(2,0),'g0':(-2,1),'g':(-1,1),'h':(0,1),'i':(1,1),'i4':(2,1),
           'g5':(-1,2),'h5':(0,2),'i5':(1,2)}
    prefix='\n'.join(f'ivec3 {n}=fetchPixel(rotateOffset(ivec2({x},{y}),rotation)).rgb;' for n,(x,y) in names.items())
    parts += [f'void xbrPixel(int rotation) {{\n{prefix}\n{body}\n}}\n',
              'void xbr() { for(int i=0;i<16;++i) pixels[i]=pack(fetchPixel(ivec2(0)).rgb);for(int r=0;r<4;++r) xbrPixel(r);}\n']
    return ''.join(parts)

HQ_RUNTIME = '''
void hq() {
 int c[10],w[10],yuv[10];c[0]=w[0]=yuv[0]=0;
 int k=1;
 for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
  w[k]=indexAt(x,y);ivec3 colour=rgb(w[k]);colour.r&=~3;colour.g&=~3;
  c[k]=int(pack(colour));yuv[k]=toYUV(c[k]);++k;
 }
 int pattern=0,flag=1;
 for(k=1;k<=9;++k) {if(k==5)continue;if(w[k]!=w[5] && Diff(yuv[5],yuv[k])) pattern|=flag;flag<<=1;}
 int edges=(Diff(yuv[4],yuv[2])?1:0)|(Diff(yuv[2],yuv[6])?2:0)|(Diff(yuv[6],yuv[8])?4:0)|(Diff(yuv[8],yuv[4])?8:0);
 int base=((settings.w-1)*4096+edges*256+pattern)*16;
 for(k=0;k<settings.z*settings.z;++k) {
  int address=base+k;
  int code=int(texelFetch(Lookup,ivec2(address%512,address/512),0).r);
  int a=c[(code>>4)&15],b=c[(code>>8)&15],third=c[(code>>12)&15],result=0;
  switch(code&15) {
   case 0:result=StoreRGB(a);break;case 1:result=Interp1(a,b);break;
   case 2:result=Interp2(a,b,third);break;case 3:result=Interp3(a,b);break;
   case 4:result=Interp4(a,b,third);break;case 5:result=Interp5(a,b);break;
   case 6:result=Interp6(a,b,third);break;case 7:result=Interp7(a,b,third);break;
   case 8:result=Interp8(a,b);break;case 9:result=Interp9(a,b,third);break;
   case 10:result=Interp10(a,b,third);break;
  }
  pixels[k]=uint(result);
 }
}
'''

def generate_lookup():
    with tempfile.TemporaryDirectory() as temporary:
        functions='''#include <iostream>
        #include <cstdint>
        #include <algorithm>
        using uint=unsigned;
        uint pixels[16];int mask;
        bool Diff(int a,int b) {
         int edge=(a==4&&b==2)?0:(a==2&&b==6)?1:(a==6&&b==8)?2:3;
         return mask&(1<<edge);
        }
        uint op(int kind,int a,int b=0,int c=0) {return kind|(a<<4)|(b<<8)|(c<<12);}
        uint StoreRGB(int a) {return op(0,a);}
        '''
        for i in range(1,11):
         functions+=f'uint Interp{i}(int a,int b'+(',int c' if i in (2,4,6,7,9,10) else '')+') {return op('+str(i)+',a,b'+(',c' if i in (2,4,6,7,9,10) else '')+');}\n'
        for f in (2,3,4):
         shader=hq(f);sw=shader[shader.index('switch (pattern)'):shader.rfind('}')]
         functions+=f'void hq{f}(int pattern) {{int c[10],yuv[10];for(int i=0;i<10;++i)c[i]=yuv[i]=i;\n'+sw+'}\n'
        functions+='''int main() {for(int alg=0;alg<3;++alg)for(mask=0;mask<16;++mask)for(int pattern=0;pattern<256;++pattern){
         std::fill_n(pixels,16,0); if(alg==0)hq2(pattern);else if(alg==1)hq3(pattern);else hq4(pattern);
         for(auto v:pixels) {char bytes[]={char(v&255),char(v>>8)};std::cout.write(bytes,2);}
        }}'''
        cpp=Path(temporary)/'hq-lut.cc';cpp.write_text(functions)
        subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O1',str(cpp),'-o',str(Path(temporary)/'hq-lut')],check=True)
        data=subprocess.check_output([str(Path(temporary)/'hq-lut')])
        words=struct.unpack('<'+'H'*(len(data)//2),data)
        (ROOT/'scalers_hq_lut.h').write_text('''// Generated from Exult's HQx tables by generate_scalers.py.
        // HQx: Copyright (C) 2003 MaxSt, LGPL-2.0-or-later; see scale_hqnx.h.
        #pragma once
        #include <cstdint>
        namespace ScaleFX { namespace Shaders {
        inline constexpr uint16_t hq_lookup[]={\n'''+''.join('    '+','.join(str(x) for x in words[i:i+16])+',\n' for i in range(0,len(words),16))+'};\n}}\n')


if __name__=='__main__':
    generate_lookup()
    s=COMMON+HQ_COMMON+HQ_RUNTIME+SAI_COMMON
    s+=''.join(sai(n) for n in ('2xSaI','Super2xSaI','SuperEagle'))+xbr()
    s=s.replace('void 2xSaI()', 'void SaI()')
    s+='''void main() {
 position=ivec2(gl_FragCoord.xy);
 for(int i=0;i<16;++i) pixels[i]=0u;
 switch(settings.w) {
 case 1:case 2:case 3:hq();break;
 case 4:SaI();break;case 5:Super2xSaI();break;case 6:SuperEagle();break;
 case 7:xbr();break;
 }
 int start=output_group.x*8;
 Pattern0=uvec4(pixels[start],pixels[start+1],pixels[start+2],pixels[start+3]);
 Pattern1=uvec4(pixels[start+4],pixels[start+5],pixels[start+6],pixels[start+7]);
}'''
    (ROOT/'scalers.frag').write_text('\n'.join(line.rstrip() for line in s.splitlines())+'\n')

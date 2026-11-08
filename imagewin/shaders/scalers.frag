#version 450
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

uint Interpolate_2xSaI(int a,int b) {return pack((rgb(a)+rgb(b))>>1);}
uint OInterpolate_2xSaI(int a,int b,int c) {return pack((rgb(a)*6+rgb(b)+rgb(c))>>3);}
uint QInterpolate_2xSaI(int a,int b,int c,int d) {return pack((rgb(a)+rgb(b)+rgb(c)+rgb(d))>>2);}
int GetResult1(int A,int B,int C,int D) {
 int x=0,y=0; if(A==C) ++x;else if(B==C) ++y;if(A==D) ++x;else if(B==D) ++y;
 return (x<=1?1:0)-(y<=1?1:0);
}
int GetResult2(int A,int B,int C,int D) {return -GetResult1(A,B,C,D);}
void SaI() {
int x=position.x-limits.z,y=position.y-limits.w;
 int sline_pixels=4096;
 int prev1_yoff=position.y>0?4096:0;
 int next1_yoff=position.y+1<limits.y?4096:0;
 int next2_yoff=min(2,max(0,limits.y-1-position.y))*4096;
 int prev1_xoff=position.x>0?1:0,next1_xoff=position.x+1<settings.x?1:0;
 int next2_xoff=min(2,max(0,settings.x-1-position.x));
 int xbeforelast=settings.x-2-limits.z;
 int xbeforelast1=settings.x-1-limits.z,xbeforelast2=settings.x-2-limits.z;
 int ybeforelast1=limits.y-1-limits.w,ybeforelast2=limits.y-2-limits.w;


			int colorA;
			int colorB;
			int colorC;
			int colorD;
			int colorE;
			int colorF;
			int colorG;
			int colorH;
			int colorI;
			int colorJ;
			int colorK;
			int colorL;
			int colorM;
			int colorN;
			int colorO ;
			uint   product;
			uint   product1;
			uint   product2;
			uint   orig;


			if (x >= xbeforelast) {
				if (x == xbeforelast) {
					next2_xoff = 1;
				} else {
					next2_xoff = next1_xoff = 0;
				}
			}






			colorI = indexAt(0 - 0 - prev1_xoff,(0 - prev1_yoff - 0)/4096);
			colorE = indexAt(0 - 0,(0 - prev1_yoff)/4096);
			colorF = indexAt(0 - 0 + next1_xoff,(0 - prev1_yoff + 0)/4096);
			colorJ = indexAt(0 - 0 + next2_xoff,(0 - prev1_yoff + 0)/4096);

			colorG = indexAt(0 - prev1_xoff,(0 - 0)/4096);
			colorA = indexAt(0,(0)/4096);
			colorB = indexAt(0 + next1_xoff,(0 + 0)/4096);
			colorK = indexAt(0 + next2_xoff,(0 + 0)/4096);

			colorH = indexAt(0 + 0 - prev1_xoff,(0 + next1_yoff - 0)/4096);
			colorC = indexAt(0 + 0,(0 + next1_yoff)/4096);
			colorD = indexAt(0 + 0 + next1_xoff,(0 + next1_yoff + 0)/4096);
			colorL = indexAt(0 + 0 + next2_xoff,(0 + next1_yoff + 0)/4096);

			colorM = indexAt(0 + 0 - prev1_xoff,(0 + next2_yoff - 0)/4096);
			colorN = indexAt(0 + 0,(0 + next2_yoff)/4096);
			colorO = indexAt(0 + 0 + next1_xoff,(0 + next2_yoff + 0)/4096);


			if ((colorA == colorD) && (colorB != colorC)) {
				if (((colorA == colorE) && (colorB == colorL))
					|| ((colorA == colorC) && (colorA == colorF) && (colorB != colorE) && (colorB == colorJ))) {

					product=pack(rgb(colorA));
				} else {

					product = Interpolate_2xSaI(colorA, colorB);
				}

				if (((colorA == colorG) && (colorC == colorO))
					|| ((colorA == colorB) && (colorA == colorH) && (colorG != colorC) && (colorC == colorM))) {

					product1=pack(rgb(colorA));
				} else {

					product1 = Interpolate_2xSaI(colorA, colorC);
				}

				product2=pack(rgb(colorA));
			} else if ((colorB == colorC) && (colorA != colorD)) {
				if (((colorB == colorF) && (colorA == colorH))
					|| ((colorB == colorE) && (colorB == colorD) && (colorA != colorF) && (colorA == colorI))) {

					product=pack(rgb(colorB));
				} else {

					product = Interpolate_2xSaI(colorA, colorB);
				}

				if (((colorC == colorH) && (colorA == colorF))
					|| ((colorC == colorG) && (colorC == colorD) && (colorA != colorH) && (colorA == colorI))) {

					product1=pack(rgb(colorC));
				} else {

					product1 = Interpolate_2xSaI(colorA, colorC);
				}

				product2=pack(rgb(colorB));
			} else if ((colorA == colorD) && (colorB == colorC)) {
				if (colorA == colorB) {

					product=pack(rgb(colorA));

					product1=pack(rgb(colorA));

					product2=pack(rgb(colorA));
				} else {
					int r = 0;

					product1 = Interpolate_2xSaI(colorA, colorC);

					product = Interpolate_2xSaI(colorA, colorB);

					r += GetResult1(colorA, colorB, colorG, colorE);
					r += GetResult2(colorB, colorA, colorK, colorF);
					r += GetResult2(colorB, colorA, colorH, colorN);
					r += GetResult1(colorA, colorB, colorL, colorO);

					if (r > 0) {

						product2=pack(rgb(colorA));
					} else if (r < 0) {

						product2=pack(rgb(colorB));
					} else {


						product2
								= QInterpolate_2xSaI(colorA, colorB, colorC, colorD);
					}
				}
			} else {

				product2 = QInterpolate_2xSaI(colorA, colorB, colorC, colorD);

				if ((colorA == colorC) && (colorA == colorF) && (colorB != colorE) && (colorB == colorJ)) {

					product=pack(rgb(colorA));
				} else if ((colorB == colorE) && (colorB == colorD) && (colorA != colorF) && (colorA == colorI)) {

					product=pack(rgb(colorB));
				} else {

					product = Interpolate_2xSaI(colorA, colorB);
				}

				if ((colorA == colorB) && (colorA == colorH) && (colorG != colorC) && (colorC == colorM)) {

					product1=pack(rgb(colorA));
				} else if ((colorC == colorG) && (colorC == colorD) && (colorA != colorH) && (colorA == colorI)) {

					product1=pack(rgb(colorC));
				} else {

					product1 = Interpolate_2xSaI(colorA, colorC);
				}
			}



			orig=pack(rgb(colorA));
			pixels[0]=orig;
			pixels[1]=product;
			pixels[2]=product1;
			pixels[3]=product2;





}
void Super2xSaI() {
int x=position.x-limits.z,y=position.y-limits.w;
 int sline_pixels=4096;
 int prev1_yoff=position.y>0?4096:0;
 int next1_yoff=position.y+1<limits.y?4096:0;
 int next2_yoff=min(2,max(0,limits.y-1-position.y))*4096;
 int prev1_xoff=position.x>0?1:0,next1_xoff=position.x+1<settings.x?1:0;
 int next2_xoff=min(2,max(0,settings.x-1-position.x));
 int xbeforelast=settings.x-2-limits.z;
 int xbeforelast1=settings.x-1-limits.z,xbeforelast2=settings.x-2-limits.z;
 int ybeforelast1=limits.y-1-limits.w,ybeforelast2=limits.y-2-limits.w;


			int color4;
			int color5;
			int color6;
			int color1;
			int color2;
			int color3;
			int colorA0;
			int colorA1;
			int colorA2;
			int colorA3;
			int colorB0;
			int colorB1;
			int colorB2;
			int colorB3;
			int colorS1;
			int colorS2;
			uint   product1a;
			uint   product1b;
			uint   product2a;
			uint   product2b;






			int add1;
			int add2;
			int sub1;
			int nextl1;
			int nextl2;
			int prevl1;

			if (x == 0) {
				sub1 = 0;
			} else {
				sub1 = 1;
			}

			if (x >= xbeforelast2) {
				add2 = 0;
			} else {
				add2 = 1;
			}

			if (x >= xbeforelast1) {
				add1 = 0;
			} else {
				add1 = 1;
			}

			if (y == 0) {
				prevl1 = 0;
			} else {
				prevl1 = sline_pixels;
			}

			if (y >= ybeforelast2) {
				nextl2 = 0;
			} else {
				nextl2 = sline_pixels;
			}

			if (y >= ybeforelast1) {
				nextl1 = 0;
			} else {
				nextl1 = sline_pixels;
			}

			colorB0 = indexAt(0 - 0 - sub1,(0 - prevl1 - 0)/4096);
			colorB1 = indexAt(0 - 0,(0 - prevl1)/4096);
			colorB2 = indexAt(0 - 0 + add1,(0 - prevl1 + 0)/4096);
			colorB3 = indexAt(0 - 0 + add1 + add2,(0 - prevl1 + 0 + 0)/4096);

			color4  = indexAt(0 - sub1,(0 - 0)/4096);
			color5  = indexAt(0,(0)/4096);
			color6  = indexAt(0 + add1,(0 + 0)/4096);
			colorS2 = indexAt(0 + add1 + add2,(0 + 0 + 0)/4096);

			color1  = indexAt(0 + 0 - sub1,(0 + nextl1 - 0)/4096);
			color2  = indexAt(0 + 0,(0 + nextl1)/4096);
			color3  = indexAt(0 + 0 + add1,(0 + nextl1 + 0)/4096);
			colorS1 = indexAt(0 + 0 + add1 + add2,(0 + nextl1 + 0 + 0)/4096);

			colorA0 = indexAt(0 + 0 + 0 - sub1,(0 + nextl1 + nextl2 - 0)/4096);
			colorA1 = indexAt(0 + 0 + 0,(0 + nextl1 + nextl2)/4096);
			colorA2 = indexAt(0 + 0 + 0 + add1,(0 + nextl1 + nextl2 + 0)/4096);
			colorA3 = indexAt(0 + 0 + 0 + add1 + add2,(0 + nextl1 + nextl2 + 0 + 0)/4096);

			if (color2 == color6 && color5 != color3) {

				product2b=pack(rgb(color2));
				product1b = product2b;
			} else if (color5 == color3 && color2 != color6) {

				product2b=pack(rgb(color5));
				product1b = product2b;
			} else if (color5 == color3 && color2 == color6) {
				int r = 0;





				r += GetResult1(color5, color6, color4, colorB1);
				r += GetResult2(color6, color5, colorA2, colorS1);
				r += GetResult2(color6, color5, color1, colorA1);
				r += GetResult1(color5, color6, colorB2, colorS2);

				if (r > 0) {

					product2b=pack(rgb(color6));
					product1b = product2b;
				} else if (r < 0) {

					product2b=pack(rgb(color5));
					product1b = product2b;
				} else {

					product1b = product2b = Interpolate_2xSaI(color5, color6);
				}

			} else {
				if (color6 == color3 && color3 == colorA1 && color2 != colorA2 && color3 != colorA0) {


					product2b = QInterpolate_2xSaI(color3, color3, color3, color2);
				} else if (color5 == color2 && color2 == colorA2 && colorA1 != color3 && color2 != colorA3) {


					product2b = QInterpolate_2xSaI(color3, color2, color2, color2);
				} else {

					product2b = Interpolate_2xSaI(color2, color3);
				}

				if (color6 == color3 && color6 == colorB1 && color5 != colorB2 && color6 != colorB0) {


					product1b = QInterpolate_2xSaI(color5, color6, color6, color6);
				} else if (color5 == color2 && color5 == colorB2 && colorB1 != color6 && color5 != colorB3) {


					product1b = QInterpolate_2xSaI(color6, color5, color5, color5);
				} else {

					product1b = Interpolate_2xSaI(color5, color6);
				}
			}

			if (color5 == color3 && color2 != color6 && color4 == color5 && color5 != colorA2) {

				product2a = Interpolate_2xSaI(color5, color2);
			} else if (color5 == color1 && color6 == color5 && color4 != color2 && color5 != colorA0) {

				product2a = Interpolate_2xSaI(color5, color2);
			} else {

				product2a=pack(rgb(color2));
			}

			if (color2 == color6 && color5 != color3 && color1 == color2 && color2 != colorB2) {

				product1a = Interpolate_2xSaI(color5, color2);
			} else if (color4 == color2 && color3 == color2 && color1 != color5 && color2 != colorB0) {

				product1a = Interpolate_2xSaI(color5, color2);
			} else {

				product1a=pack(rgb(color5));
			}

			pixels[0]=product1a;
			pixels[1]=product1b;
			pixels[2]=product2a;
			pixels[3]=product2b;




}
void SuperEagle() {
int x=position.x-limits.z,y=position.y-limits.w;
 int sline_pixels=4096;
 int prev1_yoff=position.y>0?4096:0;
 int next1_yoff=position.y+1<limits.y?4096:0;
 int next2_yoff=min(2,max(0,limits.y-1-position.y))*4096;
 int prev1_xoff=position.x>0?1:0,next1_xoff=position.x+1<settings.x?1:0;
 int next2_xoff=min(2,max(0,settings.x-1-position.x));
 int xbeforelast=settings.x-2-limits.z;
 int xbeforelast1=settings.x-1-limits.z,xbeforelast2=settings.x-2-limits.z;
 int ybeforelast1=limits.y-1-limits.w,ybeforelast2=limits.y-2-limits.w;


			int              color4;
			int              color5;
			int              color6;
			int              color1;
			int              color2;
			int              color3;
			int  colorA1;
			int  colorA2;
			int
					      colorB1;
			int  colorB2;
			int
									  colorS1;
			int  colorS2;
			uint                product1a;
			uint                product1b;
			uint                product2a;
			uint                product2b;






			int add1;
			int add2;
			int sub1;
			int nextl1;
			int nextl2;
			int prevl1;

			if (x == 0) {
				sub1 = 0;
			} else {
				sub1 = 1;
			}

			if (x >= xbeforelast2) {
				add2 = 0;
			} else {
				add2 = 1;
			}

			if (x >= xbeforelast1) {
				add1 = 0;
			} else {
				add1 = 1;
			}

			if (y == 0) {
				prevl1 = 0;
			} else {
				prevl1 = sline_pixels;
			}

			if (y >= ybeforelast2) {
				nextl2 = 0;
			} else {
				nextl2 = sline_pixels;
			}

			if (y >= ybeforelast1) {
				nextl1 = 0;
			} else {
				nextl1 = sline_pixels;
			}


			colorB1 = indexAt(0 - 0,(0 - prevl1)/4096);
			colorB2 = indexAt(0 - 0 + add1,(0 - prevl1 + 0)/4096);


			color4  = indexAt(0 - sub1,(0 - 0)/4096);
			color5  = indexAt(0,(0)/4096);
			color6  = indexAt(0 + add1,(0 + 0)/4096);
			colorS2 = indexAt(0 + add1 + add2,(0 + 0 + 0)/4096);

			color1  = indexAt(0 + 0 - sub1,(0 + nextl1 - 0)/4096);
			color2  = indexAt(0 + 0,(0 + nextl1)/4096);
			color3  = indexAt(0 + 0 + add1,(0 + nextl1 + 0)/4096);
			colorS1 = indexAt(0 + 0 + add1 + add2,(0 + nextl1 + 0 + 0)/4096);


			colorA1 = indexAt(0 + 0 + 0,(0 + nextl1 + nextl2)/4096);
			colorA2 = indexAt(0 + 0 + 0 + add1,(0 + nextl1 + nextl2 + 0)/4096);


			if (color2 == color6 && color5 != color3) {

				product2a=pack(rgb(color2));
				product1b = product2a;

				if ((color1 == color2) || (color6 == colorB2)) {


					product1a = QInterpolate_2xSaI(color2, color2, color2, color5);

				} else {

					product1a = Interpolate_2xSaI(color6, color5);
				}

				if ((color6 == colorS2) || (color2 == colorA1)) {


					product2b = QInterpolate_2xSaI(color2, color2, color2, color3);

				} else {

					product2b = Interpolate_2xSaI(color2, color3);
				}
			} else if (color5 == color3 && color2 != color6) {

				product1a=pack(rgb(color5));
				product2b = product1a;

				if ((colorB1 == color5) || (color3 == colorS1)) {


					product1b = QInterpolate_2xSaI(color5, color5, color5, color6);
				} else {

					product1b = Interpolate_2xSaI(color5, color6);
				}

				if ((color3 == colorA2) || (color4 == color5)) {


					product2a = QInterpolate_2xSaI(color2, color5, color5, color5);
				} else {

					product2a = Interpolate_2xSaI(color3, color2);
				}

			} else if (color5 == color3 && color2 == color6) {
				int r = 0;





				r += GetResult1(color5, color6, color4, colorB1);
				r += GetResult2(color6, color5, colorA2, colorS1);
				r += GetResult2(color6, color5, color1, colorA1);
				r += GetResult1(color5, color6, colorB2, colorS2);

				if (r > 0) {

					product2a=pack(rgb(color2));
					product1b = product2a;

					product1a = product2b = Interpolate_2xSaI(color5, color6);
				} else if (r < 0) {

					product1a=pack(rgb(color5));
					product2b = product1a;

					product1b = product2a = Interpolate_2xSaI(color5, color6);
				} else {

					product1a=pack(rgb(color5));
					product2b = product1a;

					product2a=pack(rgb(color2));
					product1b = product2a;
				}
			} else {




				product2b = OInterpolate_2xSaI(color3, color2, color6);
				product1a = OInterpolate_2xSaI(color5, color6, color2);





				product2a = OInterpolate_2xSaI(color2, color5, color3);
				product1b = OInterpolate_2xSaI(color6, color5, color3);
			}

			pixels[0]=product1a;
			pixels[1]=product1b;
			pixels[2]=product2a;
			pixels[3]=product2b;




}

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
const uint steepTies[15]=uint[15](0xff01f1dfu,0xffff8001u,0xc0000001u,0xffffffffu,0x1u,0xf8000000u,0xffffffffu,0xffffffffu,0x1u,0x0u,0x0u,0xffe00000u,0xffffffffu,0xffffffffu,0xfffu);
bool steep(int a,int b) {
 int left=11*a,right=5*b;
 if(left!=right) return left<right;
 int k=a/5;return (steepTies[k/32]&(1u<<uint(k%32)))!=0u;
}
void blendLineSteepAndShallow2(ivec3 col,int rotation) {

		blend(ivec2(0,1),col,1,4,rotation);
		blend(ivec2(1,0),col,1,4,rotation);
		blend(ivec2(1,1),col,5,6,rotation);

}
void blendLineShallow2(ivec3 col,int rotation) {

		blend(ivec2(0,1),col,1,4,rotation);
		blend(ivec2(1,1),col,3,4,rotation);

}
void blendLineSteep2(ivec3 col,int rotation) {

		blend(ivec2(1,0),col,1,4,rotation);
		blend(ivec2(1,1),col,3,4,rotation);

}
void blendLineDiagonal2(ivec3 col,int rotation) {

		blend(ivec2(1,1),col,1,2,rotation);

}
void blendCorner2(ivec3 col,int rotation) {


		blend(ivec2(1,1),col,21,100,rotation);


}
void blendLineSteepAndShallow3(ivec3 col,int rotation) {

		blend(ivec2(0,2),col,1,4,rotation);
		blend(ivec2(2,0),col,1,4,rotation);
		blend(ivec2(1,2),col,3,4,rotation);
		blend(ivec2(2,1),col,3,4,rotation);
		put(ivec2(2,2),col,rotation);

}
void blendLineShallow3(ivec3 col,int rotation) {

		blend(ivec2(1,2),col,3,4,rotation);
		blend(ivec2(2,1),col,1,4,rotation);
		blend(ivec2(0,2),col,1,4,rotation);
		put(ivec2(2,2),col,rotation);

}
void blendLineSteep3(ivec3 col,int rotation) {

		blend(ivec2(2,1),col,3,4,rotation);
		blend(ivec2(1,2),col,1,4,rotation);
		blend(ivec2(2,0),col,1,4,rotation);
		put(ivec2(2,2),col,rotation);

}
void blendLineDiagonal3(ivec3 col,int rotation) {

		blend(ivec2(2,2),col,7,8,rotation);
		blend(ivec2(2,1),col,1,8,rotation);
		blend(ivec2(1,2),col,1,8,rotation);

}
void blendCorner3(ivec3 col,int rotation) {


		blend(ivec2(2,2),col,45,100,rotation);





}
void blendLineSteepAndShallow4(ivec3 col,int rotation) {

		blend(ivec2(1,3),col,3,4,rotation);
		blend(ivec2(3,1),col,3,4,rotation);
		blend(ivec2(0,3),col,1,4,rotation);
		blend(ivec2(3,0),col,1,4,rotation);
		blend(ivec2(2,2),col,1,3,rotation);
		put(ivec2(3,3),col,rotation);put(ivec2(2,3),col,rotation);put(ivec2(3,2),col,rotation);

}
void blendLineShallow4(ivec3 col,int rotation) {

		blend(ivec2(3,2),col,3,4,rotation);
		blend(ivec2(1,3),col,3,4,rotation);
		blend(ivec2(2,2),col,1,4,rotation);
		blend(ivec2(0,3),col,1,4,rotation);
		put(ivec2(2,3),col,rotation);put(ivec2(3,3),col,rotation);

}
void blendLineSteep4(ivec3 col,int rotation) {

		blend(ivec2(2,3),col,3,4,rotation);
		blend(ivec2(3,1),col,3,4,rotation);
		blend(ivec2(2,2),col,1,4,rotation);
		blend(ivec2(3,0),col,1,4,rotation);
		put(ivec2(3,2),col,rotation);put(ivec2(3,3),col,rotation);

}
void blendLineDiagonal4(ivec3 col,int rotation) {

		blend(ivec2(3,2),col,1,2,rotation);
		blend(ivec2(2,3),col,1,2,rotation);
		put(ivec2(3,3),col,rotation);

}
void blendCorner4(ivec3 col,int rotation) {


		blend(ivec2(3,3),col,68,100,rotation);
		blend(ivec2(2,3),col,9,100,rotation);
		blend(ivec2(3,2),col,9,100,rotation);


}
void blendLineSteepAndShallow(ivec3 col,int rotation) {if(settings.z==2) blendLineSteepAndShallow2(col,rotation); else if(settings.z==3) blendLineSteepAndShallow3(col,rotation); else blendLineSteepAndShallow4(col,rotation);}
void blendLineShallow(ivec3 col,int rotation) {if(settings.z==2) blendLineShallow2(col,rotation); else if(settings.z==3) blendLineShallow3(col,rotation); else blendLineShallow4(col,rotation);}
void blendLineSteep(ivec3 col,int rotation) {if(settings.z==2) blendLineSteep2(col,rotation); else if(settings.z==3) blendLineSteep3(col,rotation); else blendLineSteep4(col,rotation);}
void blendLineDiagonal(ivec3 col,int rotation) {if(settings.z==2) blendLineDiagonal2(col,rotation); else if(settings.z==3) blendLineDiagonal3(col,rotation); else blendLineDiagonal4(col,rotation);}
void blendCorner(ivec3 col,int rotation) {if(settings.z==2) blendCorner2(col,rotation); else if(settings.z==3) blendCorner3(col,rotation); else blendCorner4(col,rotation);}
void xbrPixel(int rotation) {
ivec3 a1=fetchPixel(rotateOffset(ivec2(-1,-2),rotation)).rgb;
ivec3 b1=fetchPixel(rotateOffset(ivec2(0,-2),rotation)).rgb;
ivec3 c1=fetchPixel(rotateOffset(ivec2(1,-2),rotation)).rgb;
ivec3 a0=fetchPixel(rotateOffset(ivec2(-2,-1),rotation)).rgb;
ivec3 a=fetchPixel(rotateOffset(ivec2(-1,-1),rotation)).rgb;
ivec3 b=fetchPixel(rotateOffset(ivec2(0,-1),rotation)).rgb;
ivec3 c=fetchPixel(rotateOffset(ivec2(1,-1),rotation)).rgb;
ivec3 c4=fetchPixel(rotateOffset(ivec2(2,-1),rotation)).rgb;
ivec3 d0=fetchPixel(rotateOffset(ivec2(-2,0),rotation)).rgb;
ivec3 d=fetchPixel(rotateOffset(ivec2(-1,0),rotation)).rgb;
ivec3 e=fetchPixel(rotateOffset(ivec2(0,0),rotation)).rgb;
ivec3 f=fetchPixel(rotateOffset(ivec2(1,0),rotation)).rgb;
ivec3 f4=fetchPixel(rotateOffset(ivec2(2,0),rotation)).rgb;
ivec3 g0=fetchPixel(rotateOffset(ivec2(-2,1),rotation)).rgb;
ivec3 g=fetchPixel(rotateOffset(ivec2(-1,1),rotation)).rgb;
ivec3 h=fetchPixel(rotateOffset(ivec2(0,1),rotation)).rgb;
ivec3 i=fetchPixel(rotateOffset(ivec2(1,1),rotation)).rgb;
ivec3 i4=fetchPixel(rotateOffset(ivec2(2,1),rotation)).rgb;
ivec3 g5=fetchPixel(rotateOffset(ivec2(-1,2),rotation)).rgb;
ivec3 h5=fetchPixel(rotateOffset(ivec2(0,2),rotation)).rgb;
ivec3 i5=fetchPixel(rotateOffset(ivec2(1,2),rotation)).rgb;
const int    weight            = 4;


	if (e == h || e == f) {
		return;
	}

	const int hf = dist(e,c) + dist(e,g) + dist(i,h5) + dist(i,f4) + weight * dist(h,f);
	const int ei = dist(h,d) + dist(h,i5) + dist(f,i4) + dist(f,b) + weight * dist(e,i);

	ivec3 px = dist(e,f) <= dist(e,h) ? f : h;


	const int    dh                    = dist(d0,g) + dist(g,h5) + dist(a,e) + dist(e,i) + weight * dist(d,h);
	const int    eg                    = dist(b,d) + dist(d,g0) + dist(f,h) + dist(h,g5) + weight * dist(e,g);

	const int ec = dist(d,b) + dist(b,c1) + dist(h,f) + dist(f,c4) + weight * dist(e,c);
	const int bf = dist(a,e) + dist(e,i) + dist(b1,c) + dist(c,f4) + weight * dist(b,f);

	const bool doBlendLine =


			((eg < dh || equals(e,d) || equals(e,h)) && (ec < bf || equals(e,b) || equals(e,f)) &&


			 !(equals(g,h) && equals(h,i) && equals(i,f) && equals(f,c) && !equals(e,i)))
			||

			(equals(d,h) && equals(h,f) && equals(a,e) && equals(e,c) && !equals(h,e))
			|| (equals(h,f) && equals(f,b) && equals(g,e) && equals(e,a) && !equals(f,e)) || 18 * hf < 5 * ei;

	if (hf < ei) {
		if (doBlendLine) {
			const int fg  = dist(f,g);
			const int hc  = dist(h,c);
			bool      ex2 = e != c && b != c;
			bool      ex3 = e != g && d != g;

			if (steep(fg,hc) && ex3) {
				if (steep(hc,fg) && ex2) {
					blendLineSteepAndShallow(px,rotation);
				} else {
					blendLineShallow(px,rotation);
				}
			} else {
				if (steep(hc,fg) && ex2) {
					blendLineSteep(px,rotation);
				} else {
					blendLineDiagonal(px,rotation);
				}
			}
		} else {
			blendCorner(px,rotation);
		}
	}

}
void xbr() { for(int i=0;i<16;++i) pixels[i]=pack(fetchPixel(ivec2(0)).rgb);for(int r=0;r<4;++r) xbrPixel(r);}
void main() {
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
}

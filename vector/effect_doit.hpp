#if in4
#define IN4 4
#else
#define IN4 3
#endif
#if out4
#define OUT4 4
#else
#define OUT4 3
#endif
#if in_A
#define IN_A T
#else
#define IN_A F
#endif
#define Mat C4(Mat,OUT4,x,IN4)

static int C4(doit,IN4,OUT4,IN_A)(lua_State* L, const Drawable* in, Matrix* _mat, Scalar plusr, Scalar plusg, Scalar plusb, Scalar plusa) {
  Mat& mat = *(Mat*)_mat;
  int rsh, gsh, bsh, ash;
  Graphic* ret = new Graphic(in->width, in->height, in->layout);
  ret->Push(L); // will now be garbage collected
  switch(in->layout) {
  case FB_RGBx: rsh = 24; gsh = 16; bsh = 8; if(in_A||out4) ash = 0; break;
  default:
  case FB_xRGB: if(in_A||out4) ash = 24; rsh = 16; gsh = 8; bsh = 0; break;
  case FB_BGRx: bsh = 24; gsh = 16; rsh = 8; if(in_A||out4) ash = 0; break;
  case FB_xBGR: if(in_A||out4) ash = 24; bsh = 16; gsh = 8; rsh = 0; break;
  }
  for(int y = 0; y < in->height; ++y) {
    const Pixel* inp = in->rows[y];
    Pixel* outp = ret->rows[y];
    size_t rem = in->width;
#if in4 && in_A
    Scalar ri, gi, bi, ai;
#else
    Scalar ri, gi, bi;
#endif
#if out4
    uint16_t ro, go, bo, ao;
#else
    uint16_t ro, go, bo;
#endif
    while(rem-- > 0) {
      ri = (Scalar)SrgbToLinear[((*inp>>rsh)&255)];
      gi = (Scalar)SrgbToLinear[((*inp>>gsh)&255)];
      bi = (Scalar)SrgbToLinear[((*inp>>bsh)&255)];
#if in4
#if in_A
      ai = (Scalar)SrgbToLinear[((*inp>>rsh)&255)];
#else
#define ai 1.0
#endif
      ro = c16(ri*mat.xx + gi*mat.xy + bi*mat.xz + ai*mat.xw + plusr);
      go = c16(ri*mat.yx + gi*mat.yy + bi*mat.yz + ai*mat.yw + plusg);
      bo = c16(ri*mat.zx + gi*mat.zy + bi*mat.zz + ai*mat.zw + plusb);
#if out4
      ao = c16(ri*mat.wx + gi*mat.wy + bi*mat.wz + ai*mat.ww + plusa);
#endif
#else
      ro = c16(ri*mat.xx + gi*mat.xy + bi*mat.xz + plusr);
      go = c16(ri*mat.yx + gi*mat.yy + bi*mat.yz + plusg);
      bo = c16(ri*mat.zx + gi*mat.zy + bi*mat.zz + plusb);
#if out4
      ao = c16(ri*mat.wx + gi*mat.wy + bi*mat.wz + plusa);
#endif
#endif
      ++inp;
#if out4
      *outp++ = (LinearToSrgb[ro]<<rsh)|(LinearToSrgb[go]<<gsh)|(LinearToSrgb[bo]<<bsh)|(LinearToSrgb[ao]<<ash);
#else
      *outp++ = (LinearToSrgb[ro]<<rsh)|(LinearToSrgb[go]<<gsh)|(LinearToSrgb[bo]<<bsh);
#endif
#if in4 && !has_A
#undef ai
#endif
    }
  }
  if(out4) ret->CheckAlpha();
  else {
    ret->has_alpha = false;
    ret->fake_alpha = true;
  }
  return 1;
}

#undef IN4
#undef OUT4
#undef IN_A
#undef Mat

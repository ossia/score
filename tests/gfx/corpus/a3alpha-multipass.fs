/*{
  "DESCRIPTION": "Two passes: pass 0 writes straight red at alpha 0.5 into an intermediate target, pass 1 forwards it. The intermediate target must carry the alpha to the last pass.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "PASSES": [ { "TARGET": "buf" }, { } ]
}*/
void main()
{
    if(PASSINDEX == 0)
        gl_FragColor = vec4(1.0, 0.0, 0.0, 0.5);
    else
        gl_FragColor = IMG_NORM_PIXEL(buf, isf_FragNormCoord);
}

/*{
  "DESCRIPTION": "Two passes (so it runs on RenderedISFNode): pass 0 stores FRAMEINDEX / 255 in red, pass 1 shows it.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "INPUTS": [],
  "PASSES": [ { "TARGET": "frameBuf" }, {} ]
}*/
void main()
{
    if(PASSINDEX == 0)
        gl_FragColor = vec4(float(FRAMEINDEX) / 255.0, 0.0, 0.0, 1.0);
    else
        gl_FragColor = IMG_NORM_PIXEL(frameBuf, isf_FragNormCoord);
}

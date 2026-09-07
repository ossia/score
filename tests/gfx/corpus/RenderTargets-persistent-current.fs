/*{
  "ISFVSN": "2.0",
  "INPUTS": [],
  "PASSES": [
    {
      "TARGET": "history",
      "PERSISTENT": true
    },
    {}
  ]
}*/
void main(){ if(PASSINDEX==0) gl_FragColor=vec4(1.,0.,0.,1.); else gl_FragColor=IMG_NORM_PIXEL(history,vec2(.5)); }

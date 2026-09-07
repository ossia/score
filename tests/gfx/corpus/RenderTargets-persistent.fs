/*{
  "ISFVSN": "2.0",
  "INPUTS": [],
  "PASSES": [
    {
      "TARGET": "history",
      "PERSISTENT": true
    }
  ]
}*/
void main(){ gl_FragColor=vec4(isf_FragNormCoord.xy,.25,1.); }

/*{
  "ISFVSN": "2.0",
  "INPUTS": [],
  "OUTPUTS": [
    {
      "NAME": "color",
      "TYPE": "color"
    },
    {
      "NAME": "depth",
      "TYPE": "depth",
      "FORMAT": "d32f"
    }
  ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": true,
    "DEPTH_WRITE": true,
    "DEPTH_COMPARE": "less"
  }
}*/
void main(){ gl_FragDepth=.5; color=vec4(1.,0.,0.,1.); }

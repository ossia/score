/*{
  "ISFVSN": "2.0",
  "INPUTS": [],
  "OUTPUTS": [
    {
      "NAME": "depth",
      "TYPE": "depth",
      "FORMAT": "d32f"
    }
  ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": true,
    "DEPTH_WRITE": true,
    "DEPTH_COMPARE": "always"
  }
}*/
void main(){ gl_FragDepth=.75; }

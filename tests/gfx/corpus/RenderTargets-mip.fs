/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [
    {
      "TYPE": "vec4",
      "NAME": "isf_FragColor"
    }
  ],
  "PIPELINE_STATE": {
    "DEPTH_TEST": false,
    "DEPTH_WRITE": false,
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  },
  "INPUTS": [],
  "OUTPUTS": [
    {
      "NAME": "mipped",
      "TYPE": "color",
      "FORMAT": "rgba8",
      "WIDTH": 128,
      "HEIGHT": 8
    },
    {
      "NAME": "depth",
      "TYPE": "depth",
      "FORMAT": "d32f"
    }
  ],
  "EXECUTION_MODEL": {
    "TYPE": "PER_MIP",
    "TARGET": "mipped"
  }
}*/
void main(){ isf_FragColor=vec4(float(PASSINDEX)/8.,0.,1.,1.); }

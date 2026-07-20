/*{
  "DESCRIPTION": "Tests basic RAW_RASTER_PIPELINE with explicit VERTEX_INPUTS/OUTPUTS and FRAGMENT_INPUTS/OUTPUTS. Renders upstream geometry with per-vertex position and color. Connect any geometry producer with position+color semantics (e.g. 1d-no-stride.cs, csf-vertex-count-expr.cs, csf-multi-geometry.cs) to this node, then connect this node to a Window. If raw raster works: geometry visible with correct vertex colors. If broken: nothing rendered or wrong colors.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "vec4", "NAME": "color" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "v_color" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec4", "NAME": "v_color" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = v_color;
}

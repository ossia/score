/*{
  "DESCRIPTION": "Tests RAW_RASTER_PIPELINE with multiple render targets via FRAGMENT_OUTPUTS. Outputs to 2 color attachments: colorOut0 (vertex color) and colorOut1 (position encoded as color). Connect any geometry producer with position+color (e.g. 1d-no-stride.cs, csf-multi-geometry.cs) to this node, then connect each output to a Window or downstream ISF. If MRT works: two output ports, each carrying different data. If broken: only one output or second output is black.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "vec4", "NAME": "color" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "v_color" },
    { "TYPE": "vec3", "NAME": "v_pos" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec4", "NAME": "v_color" },
    { "TYPE": "vec3", "NAME": "v_pos" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "colorOut0" },
    { "TYPE": "vec4", "NAME": "colorOut1" }
  ],
  "INPUTS": []
}*/

void main()
{
    // Output 0: vertex color passthrough
    colorOut0 = v_color;

    // Output 1: encode position as color (pseudo-normal)
    colorOut1 = vec4(normalize(v_pos) * 0.5 + 0.5, 1.0);
}

/*{
  "DESCRIPTION": "Tests RAW_RASTER_PIPELINE with AUXILIARY SSBOs from upstream geometry. The fragment shader reads a structured SSBO ('stats') that travels with the geometry. Connect csf-auxiliary-buffer.cs (which produces geometry with a 'stats' auxiliary containing frameCount/avgX/avgY) to this node, then this node to a Window. If AUXILIARY works: particles rendered with cycling tint from stats.frameCount. If broken: tint is zero/black or shader fails to compile.",
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
    { "TYPE": "vec3", "NAME": "v_worldPos" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec4", "NAME": "v_color" },
    { "TYPE": "vec3", "NAME": "v_worldPos" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "AUXILIARY": [
    {
      "NAME": "stats",
      "ACCESS": "read_only",
      "LAYOUT": [
        { "NAME": "frameCount", "TYPE": "uint" },
        { "NAME": "avgX", "TYPE": "float" },
        { "NAME": "avgY", "TYPE": "float" }
      ]
    }
  ],
  "INPUTS": []
}*/

void main()
{
    // Use auxiliary data to modulate the color
    float hue = fract(float(stats.frameCount) / 60.0);
    vec3 tint = vec3(
        0.5 + 0.5 * sin(hue * 6.28),
        0.5 + 0.5 * sin(hue * 6.28 + 2.09),
        0.5 + 0.5 * sin(hue * 6.28 + 4.19)
    );

    isf_FragColor = vec4(v_color.rgb * tint, v_color.a);
}

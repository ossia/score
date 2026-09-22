/*{
  "DESCRIPTION": "Guide example 2 -- raw raster on the GEOMETRY path. Draws upstream geometry placed by MODEL_MATRIX and viewed through VIEWPROJECTION_MATRIX. On the geometry path an unresolved camera binds an identity stand-in, so the camera term costs nothing until a Camera exists; on a scene chain it binds the real one. Shades by the interpolated vertex colour so a pixel oracle measures placement, not lighting.",
  "CREDIT": "score shader guide",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["GUIDE"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "vec4", "NAME": "color" }
  ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_color" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = v_color;
}

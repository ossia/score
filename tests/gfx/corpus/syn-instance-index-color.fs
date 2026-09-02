/*{
  "DESCRIPTION": "RAW_RASTER_PIPELINE consumer for syn-instance-count-user.cs: draws each instance's quad with R = 1 (drawn-coverage marker), G = the instance identity stored in the translation buffer (translation.w == i/255, forwarded as v_buf_id), B = the draw call's instance identity (gl_InstanceIndex/255, forwarded as a flat int varying), A = 1. On a plain non-sRGB RGBA8 target both identity channels read back as exactly i. G and B disagreeing means the instance buffer was reallocated/resized without rewriting its contents; a strip present with neither means a stale draw.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER", "TEST-INSTANCING"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "vec4", "NAME": "translation", "SEMANTIC": "translation" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "float", "NAME": "v_buf_id" },
    { "TYPE": "int",   "NAME": "v_draw_id", "INTERPOLATION": "flat" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "float", "NAME": "v_buf_id" },
    { "TYPE": "int",   "NAME": "v_draw_id", "INTERPOLATION": "flat" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "INPUTS": []
}*/

void main()
{
    isf_FragColor
        = vec4(1.0, v_buf_id, float(v_draw_id) / 255.0, 1.0);
}

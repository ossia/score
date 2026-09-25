/*{
  "CREDIT": "test",
  "ISFVSN": "2",
  "DESCRIPTION": "Geometry filter that declares its own #define PI, as user shaders do, and scales every vertex by sin(PI * k): k = 0.5 leaves the mesh as it is, k = 1 collapses it to a point. ModelDisplay splices filter code before its projection code, so any PI that code declares breaks the vertex stage. Used by GfxDefinePiModelDisplayF3.",
  "MODE": "GEOMETRY_FILTER",
  "CATEGORIES": [ "TEST" ],
  "INPUTS": [
    { "NAME": "k", "TYPE": "float", "DEFAULT": 0.5, "MIN": 0.0, "MAX": 1.0 }
  ]
}*/
#define PI 3.1415926535

void process_vertex(inout vec3 position, inout vec3 normal, inout vec2 uv, inout vec3 tangent, inout vec4 color)
{
  position *= sin(PI * this_filter.k);
}

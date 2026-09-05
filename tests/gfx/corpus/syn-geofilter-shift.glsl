/*{
  "CREDIT": "test",
  "ISFVSN": "2",
  "DESCRIPTION": "Geometry filter that displaces every vertex along +X by a control. Drawn through the raster the whole silhouette moves, so a test can assert WHERE the mesh landed rather than merely that something drew -- which is the only way to tell a filter that ran from one that was skipped, since a skipped filter still passes the mesh through intact.",
  "MODE": "GEOMETRY_FILTER",
  "CATEGORIES": [ "TEST-SYNTHETIC" ],
  "INPUTS": [
    { "NAME": "shift", "TYPE": "float", "DEFAULT": 0.0, "MIN": -10.0, "MAX": 10.0 }
  ]
}*/

void process_vertex(inout vec3 position, inout vec3 normal, inout vec2 uv, inout vec3 tangent, inout vec4 color)
{
  position.x += this_filter.shift;
}

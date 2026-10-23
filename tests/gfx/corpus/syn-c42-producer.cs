/*{
  "DESCRIPTION": "C42 producer: a full-screen quad (6 vertices) with three attributes whose types have no vertex format. 'packet' is a TYPES struct Packet { vec4 value; float tag; } holding green and 7; 'flag' is a bool (true); 'xform' is a mat4 (identity). They must be published as what they are, not as float4.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-GEOMETRY"],
  "TYPES": [
    { "NAME": "Packet", "LAYOUT": [ { "NAME": "value", "TYPE": "vec4" }, { "NAME": "tag", "TYPE": "float" } ] }
  ],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "6",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4",   "ACCESS": "write_only" },
        { "NAME": "packet",   "SEMANTIC": "packet",   "TYPE": "Packet", "ACCESS": "write_only" },
        { "NAME": "flag",     "SEMANTIC": "flag",     "TYPE": "bool",   "ACCESS": "write_only" },
        { "NAME": "xform",    "SEMANTIC": "xform",    "TYPE": "mat4",   "ACCESS": "write_only" }
      ]
    }
  ],
  "PASSES": [ { "LOCAL_SIZE": [6, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } } ]
}*/

void main()
{
  uint i = gl_GlobalInvocationID.x;
  if(i >= 6u)
    return;
  const vec2 corners[6] = vec2[6](
      vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(1, 1), vec2(-1, 1));
  ISF_WRITE(geo, position)[i] = vec4(corners[i], 0.0, 1.0);
  Packet p;
  p.value = vec4(0.0, 1.0, 0.0, 1.0);
  p.tag = 7.0;
  ISF_WRITE(geo, packet)[i] = p;
  ISF_WRITE(geo, flag)[i] = true;
  ISF_WRITE(geo, xform)[i] = mat4(1.0);
}

/*{
  "DESCRIPTION": "Paints what the RUNTIME accessors of its input read, one query per pixel, as value + 1 in red, 0 for the accessors' invalid result. Row 0: ISF_INDEX(x). Row 1: ISF_TRIANGLE_INDEX(x / 3, x % 3). Row 2: ISF_INSTANCE_INDEX(x). Row 3: ISF_VERTEX_INDEX(x). Row 4: the element, triangle, instance and vertex counts, then ISF_HAS_ATTRIBUTE of position, weight and tag.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "16", "HEIGHT": "8" },
    { "NAME": "geo", "TYPE": "geometry", "RUNTIME": true,
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only", "REQUIRED": false },
        { "NAME": "weight", "TYPE": "float", "ACCESS": "read_only", "REQUIRED": false },
        { "NAME": "tag", "TYPE": "uint", "ACCESS": "read_only", "REQUIRED": false }
      ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [16, 8, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } } ]
}*/
float encode(uint v)
{
    return v == 0xffffffffu ? 0.0 : float(min(v + 1u, 255u)) / 255.0;
}

void main()
{
    uvec2 p = gl_GlobalInvocationID.xy;
    uint x = p.x;
    uint v = 0xffffffffu;
    if (p.y == 0u)
        v = ISF_INDEX(geo, x);
    else if (p.y == 1u)
        v = ISF_TRIANGLE_INDEX(geo, x / 3u, x % 3u);
    else if (p.y == 2u)
        v = ISF_INSTANCE_INDEX(geo, x);
    else if (p.y == 3u)
        v = ISF_VERTEX_INDEX(geo, x);
    else if (p.y == 4u)
    {
        if (x == 0u) v = ISF_ELEMENT_COUNT(geo);
        else if (x == 1u) v = ISF_TRIANGLE_COUNT(geo);
        else if (x == 2u) v = ISF_INSTANCE_COUNT(geo);
        else if (x == 3u) v = ISF_VERTEX_COUNT(geo);
        else if (x == 4u) v = ISF_HAS_ATTRIBUTE(geo, position) ? 1u : 0u;
        else if (x == 5u) v = ISF_HAS_ATTRIBUTE(geo, weight) ? 1u : 0u;
        else if (x == 6u) v = ISF_HAS_ATTRIBUTE(geo, tag) ? 1u : 0u;
    }
    IMG_STORE(outputImage, ivec2(p), vec4(encode(v), 0.0, 0.0, 1.0));
}

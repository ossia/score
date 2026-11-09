/*{
  "DESCRIPTION": "Paints what its optional attributes read at every vertex: red = weight, green = offset.y, blue = 1 when label, mask, scale, the read_write pair and plain all read their DEFAULT (or zeros), alpha = 1. A vertex reading otherwise paints black. pair is written back unchanged.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "16", "HEIGHT": "16" },
    { "NAME": "geoIn", "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" },
        { "NAME": "weight", "TYPE": "float", "ACCESS": "read_only", "REQUIRED": false, "DEFAULT": 1 },
        { "NAME": "offset", "TYPE": "vec3", "ACCESS": "read_only", "REQUIRED": false, "DEFAULT": [0.25, 0.5, 0.75] },
        { "NAME": "label", "TYPE": "int", "ACCESS": "read_only", "REQUIRED": false, "DEFAULT": -3 },
        { "NAME": "mask", "TYPE": "uvec2", "ACCESS": "read_only", "REQUIRED": false, "DEFAULT": 7 },
        { "NAME": "scale", "TYPE": "vec4", "ACCESS": "read_only", "RATE": "instance", "REQUIRED": false, "DEFAULT": [2, 3, 4, 5] },
        { "NAME": "pair", "TYPE": "vec2", "ACCESS": "read_write", "REQUIRED": false, "DEFAULT": [0.25, 0.75] },
        { "NAME": "plain", "TYPE": "float", "ACCESS": "read_only", "REQUIRED": false }
      ] }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= imageSize(outputImage).x || p.y >= imageSize(outputImage).y) return;
    uint n = uint(ISF_READ(geoIn, position).length());
    bool same = n == 4u
        && uint(ISF_READ(geoIn, weight).length()) >= n
        && uint(ISF_READ(geoIn, offset).length()) >= n
        && uint(ISF_READ(geoIn, label).length()) >= n
        && uint(ISF_READ(geoIn, mask).length()) >= n;
    float w = ISF_READ(geoIn, weight)[0];
    vec3 o = ISF_READ(geoIn, offset)[0];
    for (uint i = 1u; i < n; ++i)
        same = same && ISF_READ(geoIn, weight)[i] == w && ISF_READ(geoIn, offset)[i] == o;
    bool defaults = true;
    for (uint i = 0u; i < n; ++i)
        defaults = defaults && ISF_READ(geoIn, label)[i] == -3
            && ISF_READ(geoIn, mask)[i] == uvec2(7u)
            && ISF_READ(geoIn, plain)[i] == 0.0
            && ISF_READ(geoIn, pair)[i] == vec2(0.25, 0.75);
    defaults = defaults && ISF_READ(geoIn, scale)[0] == vec4(2, 3, 4, 5);
    if (p.y == 0 && uint(p.x) < n)
        ISF_WRITE(geoIn, pair)[p.x] = ISF_READ(geoIn, pair)[p.x];
    IMG_STORE(outputImage, p, same ? vec4(w, o.y, defaults ? 1.0 : 0.0, 1.0) : vec4(0, 0, 0, 1));
}

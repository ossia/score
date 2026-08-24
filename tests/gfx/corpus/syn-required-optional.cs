/*{
  "DESCRIPTION": "REQUIRED false on an attribute the upstream geometry does not provide. The shader reads it anyway; the contract is a zero-filled fallback rather than a build failure. Writes the read-back value into the colour so the frame distinguishes 'fell back to zero' (black-ish, the contract) from 'refused to build' (an error) and from 'garbage' (anything else).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-REQUIRED"],
  "RESOURCES": [
    {
      "NAME": "geoOut",
      "TYPE": "geometry",
      "VERTEX_COUNT": "$VERTEX_COUNT_geoIn",
      "ATTRIBUTES": [
        { "NAME": "out_position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" },
        { "NAME": "out_color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "write_only" }
      ]
    },
    {
      "NAME": "geoIn",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "in_position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" },
        { "NAME": "in_texcoord", "SEMANTIC": "texcoord", "TYPE": "vec4", "ACCESS": "read_only", "REQUIRED": false }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } }
  ]
}*/

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    uint count = uint(ISF_READ(geoIn, in_position).length());
    if(idx >= count)
        return;

    ISF_WRITE(geoOut, out_position)[idx] = ISF_READ(geoIn, in_position)[idx];

    // The optional attribute is absent upstream: the contract is zeroes, so
    // this must read (0,0,0,0) and the blue channel below stays 0.
    vec4 uv = ISF_READ(geoIn, in_texcoord)[idx];
    ISF_WRITE(geoOut, out_color)[idx] = vec4(0.0, 1.0, uv.x, 1.0);
}

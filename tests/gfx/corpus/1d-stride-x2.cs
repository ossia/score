/*{
  "DESCRIPTION": "1D_BUFFER with STRIDE_X=2. Each thread writes 2 particles. Should display identical gradient to baseline (green left, red right, 256 particles). Black gaps = stride dispatch is wrong.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-STRIDE"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "256",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_write" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "read_write" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX", "TARGET": "geo", "STRIDE_X": 2 } }
  ]
}*/

#define STRIDE 2

void main()
{
    uint base = gl_GlobalInvocationID.x * uint(STRIDE);
    uint count = uint(geo_position_out.length());

    for(uint i = 0u; i < uint(STRIDE); i++)
    {
        uint idx = base + i;
        if(idx >= count)
            return;

        float t = float(idx) / float(count - 1u);

        geo_position_out[idx] = vec4(t * 2.0 - 1.0, 0.0, 0.0, 1.0);
        geo_color_out[idx] = vec4(t, 1.0 - t, 0.0, 1.0);
    }
}

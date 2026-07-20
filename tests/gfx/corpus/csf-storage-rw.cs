/*{
  "DESCRIPTION": "Tests CSF storage buffer (SSBO) with LAYOUT. Pipeline: this -> raw-raster-basic.fs -> Window. Pass 0: writes position+velocity into SSBO. Pass 1: reads SSBO and writes to geometry. If storage works: 64 rotating particles in a circle. If broken: particles at origin or no output.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-STORAGE"],
  "RESOURCES": [
    {
      "NAME": "buf",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "LAYOUT": [
        { "NAME": "positions", "TYPE": "vec4[64]" },
        { "NAME": "velocities", "TYPE": "vec4[64]" },
        { "NAME": "count", "TYPE": "uint" }
      ]
    },
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "64",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" },
        { "NAME": "color",    "SEMANTIC": "color",    "TYPE": "vec4", "ACCESS": "write_only" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "1D_BUFFER", "TARGET": "buf" } },
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX", "TARGET": "geo" } }
  ]
}*/

void main()
{
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= 64u)
        return;

    if(PASSINDEX == 0)
    {
        // Write structured data to storage buffer
        float angle = float(idx) / 64.0 * 6.28318 + TIME;
        vec4 pos = vec4(cos(angle) * 2.0, sin(angle) * 2.0, 0.0, 1.0);
        vec4 vel = vec4(-sin(angle), cos(angle), 0.0, 0.0);

        buf.positions[idx] = pos;
        buf.velocities[idx] = vel;
        if(idx == 0u)
            buf.count = 64u;
    }
    else
    {
        // Read from storage, write to geometry output
        vec4 pos = buf.positions[idx];
        vec4 vel = buf.velocities[idx];

        geo_position_out[idx] = pos;

        // Color from velocity direction
        geo_color_out[idx] = vec4(
            abs(vel.x),
            abs(vel.y),
            0.5,
            1.0
        );
    }
}

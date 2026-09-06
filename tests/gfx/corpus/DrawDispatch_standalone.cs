/*{
  "DESCRIPTION": "DrawDispatch-4: standalone BUFFER_USAGE indirect draw pickup must initialize command stride.",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "6",
      "INSTANCE_COUNT": "8",
      "ATTRIBUTES": [
        {
          "NAME": "position",
          "SEMANTIC": "position",
          "TYPE": "vec4",
          "ACCESS": "write_only",
          "RATE": "vertex"
        },
        {
          "NAME": "translation",
          "SEMANTIC": "translation",
          "TYPE": "vec4",
          "ACCESS": "write_only",
          "RATE": "instance"
        }
      ]
    },
    {
      "NAME": "drawargs",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "BUFFER_USAGE": "indirect_draw",
      "LAYOUT": [
        {
          "NAME": "cmd",
          "TYPE": "uint[5]"
        }
      ]
    }
  ],
  "PASSES": [
    {
      "LOCAL_SIZE": [
        6,
        1,
        1
      ],
      "EXECUTION_MODEL": {
        "TYPE": "PER_VERTEX",
        "TARGET": "geo"
      }
    },
    {
      "LOCAL_SIZE": [
        8,
        1,
        1
      ],
      "EXECUTION_MODEL": {
        "TYPE": "PER_INSTANCE",
        "TARGET": "geo"
      }
    }
  ]
}*/
void main()
{
  uint i = gl_GlobalInvocationID.x;
  if(PASSINDEX == 0)
  {
    if(i >= 6u) return;
    vec2 p = vec2(-1.0, -1.0);
    if(i == 1u) p = vec2(-0.875, -1.0);
    if(i == 2u || i == 4u) p = vec2(-0.875, 1.0);
    if(i == 5u) p = vec2(-1.0, 1.0);
    geo_position_out[i] = vec4(p, 0.0, 1.0);
    if(i == 0u)
    {
      drawargs.cmd[0] = 6u;
      drawargs.cmd[1] = 2u;
      drawargs.cmd[2] = 0u;
      drawargs.cmd[3] = 0u;
      drawargs.cmd[4] = 0u;
    }
  }
  else if(i < 8u)
    geo_translation_out[i] = vec4(float(i) * 0.125, 0.0, 0.0, float(i) / 255.0);
}

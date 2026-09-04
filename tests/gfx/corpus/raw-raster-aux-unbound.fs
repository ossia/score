/*{
  "DESCRIPTION": "An unbound top-level AUXILIARY must read back as zeros. Two AUXILIARY blocks -- one std430 SSBO, one std140 UBO -- that no upstream geometry publishes, so the engine allocates a placeholder for each. Shaders read those placeholders as SENTINELS (classic_pbr_openpbr gates its whole clustered-lighting and volumetric paths on `cluster_config.cluster_x == 0u`), so the placeholder has to be zero-filled: Vulkan does not initialise VkBuffer memory, and on a RenderList rebuild the fresh placeholder lands on whatever the previous owner of that suballocation left there. Green = both sentinels read 0. Red = at least one came back with recycled device memory.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "vec4", "NAME": "color" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "v_color" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec4", "NAME": "v_color" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "AUXILIARY": [
    {
      "NAME": "probe_ssbo",
      "ACCESS": "read_only",
      "LAYOUT": [
        { "NAME": "magic_a", "TYPE": "uint" },
        { "NAME": "magic_b", "TYPE": "uint" },
        { "NAME": "magic_c", "TYPE": "uint" },
        { "NAME": "magic_d", "TYPE": "uint" }
      ]
    },
    {
      "NAME": "probe_ubo",
      "TYPE": "uniform",
      "LAYOUT": [
        { "NAME": "magic_e", "TYPE": "uint" },
        { "NAME": "magic_f", "TYPE": "uint" },
        { "NAME": "magic_g", "TYPE": "uint" },
        { "NAME": "magic_h", "TYPE": "uint" }
      ]
    }
  ],
  "INPUTS": []
}*/

void main()
{
    uint acc = probe_ssbo.magic_a | probe_ssbo.magic_b
             | probe_ssbo.magic_c | probe_ssbo.magic_d
             | probe_ubo.magic_e  | probe_ubo.magic_f
             | probe_ubo.magic_g  | probe_ubo.magic_h;

    // Green when every sentinel is the zero the engine promises, red when any
    // of them came back as recycled device memory.
    vec3 rgb = (acc == 0u) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);

    // v_color is referenced (clamped away) only to keep the vertex->fragment
    // interface identical to raw-raster-basic; the verdict is in rgb.
    isf_FragColor = vec4(rgb, clamp(v_color.a, 1.0, 1.0));
}

/*{
  "DESCRIPTION": "Reaction-Diffusion: Map chemical concentrations to colors for visualization",
  "CREDIT": "ossia score",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["Reaction-Diffusion"],
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" },
        { "NAME": "chemical", "TYPE": "vec2", "ACCESS": "read_only" },
        { "NAME": "color", "SEMANTIC": "color", "TYPE": "vec4", "ACCESS": "write_only" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "1D_BUFFER" } }
  ]
}*/

// Map (U, V) concentrations to a color for visualization.
// High V (activator) -- bright, low V -- dark background.

void main()
{
  uint idx = gl_GlobalInvocationID.x;
  if(idx >= ISF_READ(geo, chemical).length()) return;

  vec2 c = geo_chemical_in[idx];
  float U = c.x;
  float V = c.y;

  // Color mapping: V drives the main visual pattern
  // Dark blue background (high U, low V) -- white/cyan spots (high V)
  vec3 bg = vec3(0.0, 0.02, 0.1);    // deep blue
  vec3 fg = vec3(0.9, 0.95, 1.0);     // white-cyan

  float t = smoothstep(0.0, 0.5, V);
  vec3 col = mix(bg, fg, t);

  // Subtle tint based on U for extra visual info
  col += vec3(0.1, 0.0, 0.0) * (1.0 - U) * 0.3;

  geo_color_out[idx] = vec4(col, 1.0);

  // Pass through position unchanged
  // (position is read_only, downstream gets it via attribute forwarding)
}

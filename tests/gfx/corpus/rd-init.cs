/*{
  "DESCRIPTION": "Reaction-Diffusion: Source node (TD-style hub). Seeds initial state on frame 0 or reset, passes through feedback data otherwise.",
  "CREDIT": "ossia score",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["Reaction-Diffusion"],
  "RESOURCES": [
    { "NAME": "gridWidth", "TYPE": "long", "DEFAULT": 128, "MIN": 16, "MAX": 512 },
    { "NAME": "reset", "TYPE": "event" },
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "$gridWidth * $gridWidth",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_write" },
        { "NAME": "chemical", "TYPE": "vec2", "ACCESS": "read_write" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "1D_BUFFER" } }
  ]
}*/

// TouchDesigner-style "Particle POP" pattern:
// - On first frame (FRAMEINDEX == 0) or reset pulse: seed initial data
// - On subsequent frames: pass through feedback data unchanged
//
// Connect feedback from React back to this node's geometry input.
// The feedback gives this node last frame's evolved state.
// Without feedback, this node re-initializes every frame (no evolution).

void main()
{
  uint idx = gl_GlobalInvocationID.x;
  int W = gridWidth;
  int total = W * W;
  if(idx >= total) return;

  // On subsequent frames with feedback: pass through (data already in _in, copy to _out)
  if(FRAMEINDEX > 1 && reset == false)
  {
    geo_position_out[idx] = geo_position_in[idx];
    geo_chemical_out[idx] = geo_chemical_in[idx];
    return;
  }

  // First frame or reset: seed the grid
  int x = int(idx) % W;
  int y = int(idx) / W;

  // Position: map to [-1, 1]
  vec2 p = vec2(float(x) / float(W - 1), float(y) / float(W - 1)) * 2.0 - 1.0;
  geo_position_out[idx] = vec4(p, 0.0, 1.0);

  // Chemical: U = 1.0, V = 0.0 everywhere
  float U = 1.0;
  float V = 0.0;

  // Seed: square region in center with some noise
  vec2 center = vec2(float(W) / 2.0);
  float dist = max(abs(float(x) - center.x), abs(float(y) - center.y));
  float seedRadius = float(W) * 0.1;

  if(dist < seedRadius)
  {
    // Simple hash for per-cell noise
    float noise = fract(sin(float(idx) * 12.9898 + 78.233) * 43758.5453);
    U = 0.5 + 0.02 * noise;
    V = 0.25 + 0.02 * noise;
  }

  geo_chemical_out[idx] = vec2(U, V);
}

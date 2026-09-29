/*{
  "DESCRIPTION": "Reaction-Diffusion: Diffusion step (Laplacian). Connect React output back to this node's input for feedback -- without it, neighbor reads are racy.",
  "CREDIT": "ossia score",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["Reaction-Diffusion"],
  "RESOURCES": [
    { "NAME": "gridWidth", "TYPE": "long", "DEFAULT": 128, "MIN": 16, "MAX": 512 },
    { "NAME": "diffU", "TYPE": "float", "DEFAULT": 0.16, "MIN": 0.0, "MAX": 1.0 },
    { "NAME": "diffV", "TYPE": "float", "DEFAULT": 0.08, "MIN": 0.0, "MAX": 1.0 },
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" },
        { "NAME": "chemical", "TYPE": "vec2", "ACCESS": "gather" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "1D_BUFFER" } }
  ]
}*/

// Diffusion step: 5-point Laplacian stencil on a 2D grid.
//
// WHY FEEDBACK MATTERS HERE:
// The Laplacian reads chemical[i-1], chemical[i+1], chemical[i-W], chemical[i+W].
// Without feedback (in-place mode), a neighboring thread may have already written
// its new value before we read it -- the stencil mixes old and new data depending
// on dispatch order. This corrupts the diffusion.
// With feedback, _in is a frozen snapshot of last frame. All reads are consistent.

void main()
{
  uint idx = gl_GlobalInvocationID.x;
  int W = gridWidth;
  int total = W * W;
  if(idx >= total) return;

  int x = int(idx) % W;
  int y = int(idx) / W;

  // Read center value
  vec2 c = geo_chemical_in[idx];

  // Read 4 neighbors with wrapping (toroidal boundary)
  int xm = (x - 1 + W) % W;
  int xp = (x + 1) % W;
  int ym = (y - 1 + W) % W;
  int yp = (y + 1) % W;

  vec2 left  = geo_chemical_in[ym * W + x];  // Note: this is actually "up" but naming doesn't matter
  vec2 right = geo_chemical_in[yp * W + x];
  vec2 down  = geo_chemical_in[y * W + xm];
  vec2 up    = geo_chemical_in[y * W + xp];

  // 5-point Laplacian
  vec2 lap = left + right + up + down - 4.0 * c;

  // Apply diffusion
  vec2 result;
  result.x = c.x + diffU * lap.x;
  result.y = c.y + diffV * lap.y;

  geo_chemical_out[idx] = result;
}

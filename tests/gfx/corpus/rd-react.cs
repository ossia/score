/*{
  "DESCRIPTION": "Reaction-Diffusion: Gray-Scott reaction step. Apply after diffusion.",
  "CREDIT": "ossia score",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["Reaction-Diffusion"],
  "RESOURCES": [
    { "NAME": "feed", "TYPE": "float", "DEFAULT": 0.035, "MIN": 0.0, "MAX": 0.1 },
    { "NAME": "kill", "TYPE": "float", "DEFAULT": 0.065, "MIN": 0.0, "MAX": 0.1 },
    { "NAME": "dt", "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.1, "MAX": 5.0 },
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "read_only" },
        { "NAME": "chemical", "TYPE": "vec2", "ACCESS": "read_write" }
      ]
    }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [64, 1, 1], "EXECUTION_MODEL": { "TYPE": "1D_BUFFER" } }
  ]
}*/

// Gray-Scott reaction:
//   dU/dt = -U*V*V + f*(1 - U)
//   dV/dt = +U*V*V - (f + k)*V
//
// This step only touches the local cell -- no neighbor reads -- so it works
// correctly both with and without feedback. The point of separating it from
// diffusion is to show that diffusion (which reads neighbors) is the step
// that breaks without proper double-buffering.

void main()
{
  uint idx = gl_GlobalInvocationID.x;
  if(idx >= ISF_READ(geo, chemical).length()) return;

  vec2 c = geo_chemical_in[idx];
  float U = c.x;
  float V = c.y;

  float UVV = U * V * V;
  U += dt * (-UVV + feed * (1.0 - U));
  V += dt * ( UVV - (feed + kill) * V);

  // Clamp to valid range
  geo_chemical_out[idx] = clamp(vec2(U, V), 0.0, 1.0);
}

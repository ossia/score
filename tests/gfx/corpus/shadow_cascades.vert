// Shadow-pass vertex shader. Transforms each drawable's vertex into the
// cascade's light-space clip-space.
//
// Iteration: EXECUTION_MODEL=PER_LAYER (declared in the .frag JSON
// header) drives the runtime to issue one draw per array layer of the
// `shadow` depth output. The iteration index lands in `PASSINDEX` (the
// standard ISF process-UBO field, binding=1) and we use it directly as
// the cascade index — no separate per-pass UBO needed.
//
// Bindings:
//   - `shadow_cascades` UBO: scene-wide, published by ScenePreprocessor
//     as the auxiliary buffer named "shadow_cascades". Carries the matrix
//     ARRAY + split distances + cascade_count; shared with
//     classic_pbr_shadowed for fragment-side cascade sampling.
//   - `PASSINDEX`: built-in. Iteration index 0..cascade_count-1.

void main()
{
    uint drawId = draw_id;
    PerDraw pd = per_draws.data[drawId];
    mat4 model = pd.model;

    vec4 wp = model * vec4(position, 1.0);
    mat4 lightVP = shadow_cascades.light_view_proj[PASSINDEX];

    gl_Position = clipSpaceCorrMatrix * lightVP * wp;
}

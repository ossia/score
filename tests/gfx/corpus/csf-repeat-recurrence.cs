/*{
  "DESCRIPTION": "REPEAT groups driving an order-dependent recurrence. Each dispatch of passes 1 and 2 folds its ITERATIONINDEX into acc and logs ITERATIONINDEX * 16 + PASSINDEX; pass 3 is in a group repeated 0 times; pass 4 sits in a group of 2 nested in a group of 2 and logs 100 + ITERATIONINDEX * 10 + PASSINDEX. Pass 5 paints the log in row 0 and acc, then the log length, in row 1, as value / 255 in red.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "iterations", "TYPE": "long", "DEFAULT": 3, "MIN": 0, "MAX": 12 },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "16", "HEIGHT": "2" },
    { "NAME": "state", "TYPE": "storage", "ACCESS": "read_write",
      "LAYOUT": [ { "NAME": "acc", "TYPE": "uint" }, { "NAME": "n", "TYPE": "uint" }, { "NAME": "log", "TYPE": "uint[32]" } ] }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } },
    { "REPEAT": "$iterations", "PASSES": [
      { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } },
      { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } }
    ] },
    { "REPEAT": 0, "PASSES": [
      { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } }
    ] },
    { "REPEAT": 2, "PASSES": [
      { "REPEAT": "1 + 1", "PASSES": [
        { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } }
      ] }
    ] },
    { "LOCAL_SIZE": [16, 2, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } }
  ]
}*/
void append(uint v)
{
    if (state.n < 32u)
        state.log[state.n] = v;
    state.n += 1u;
}

void main()
{
    uint it = uint(ITERATIONINDEX);
    if (PASSINDEX == 0)
    {
        state.acc = 0u;
        state.n = 0u;
        return;
    }
    if (PASSINDEX == 1)
    {
        state.acc = (state.acc * 3u + it + 1u) % 251u;
        return;
    }
    if (PASSINDEX == 2)
    {
        state.acc = (state.acc * 5u + 2u) % 251u;
        append(it * 16u + 2u);
        return;
    }
    if (PASSINDEX == 3)
    {
        append(200u);
        return;
    }
    if (PASSINDEX == 4)
    {
        append(100u + it * 10u + 4u);
        return;
    }
    uvec2 p = gl_GlobalInvocationID.xy;
    uint v = 0u;
    if (p.y == 0u)
        v = p.x < min(state.n, 32u) ? state.log[p.x] : 0u;
    else if (p.x == 0u)
        v = state.acc;
    else if (p.x == 1u)
        v = state.n;
    IMG_STORE(outputImage, ivec2(p), vec4(float(v) / 255.0, 0.0, 0.0, 1.0));
}

/*{
  "DESCRIPTION": "Tests all ISF time-related built-in uniforms: TIME, TIMEDELTA, PROGRESS, FRAMEINDEX, RENDERSIZE, DATE. Displays each value as a colored bar or text-like indicator. TIME: horizontal sweep. TIMEDELTA: brightness (should flicker if dt varies). PROGRESS: vertical fill. FRAMEINDEX: cycling hue. RENDERSIZE: encoded as blue channel. DATE: year/month/day/seconds bars.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-UNIFORMS"],
  "INPUTS": []
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;
    int row = int(floor(uv.y * 6.0));
    vec2 cellUV = vec2(uv.x, fract(uv.y * 6.0));

    vec4 c = vec4(0.0, 0.0, 0.0, 1.0);

    // Row 0: TIME - horizontal sweeping bar (wraps every 10 seconds)
    if(row == 0)
    {
        float t = fract(TIME / 10.0);
        float bar = smoothstep(0.0, 0.02, cellUV.x - t + 0.02)
                  * smoothstep(0.0, 0.02, t + 0.02 - cellUV.x);
        c = vec4(1.0, 0.3, 0.0, 1.0) * bar + vec4(0.1) * (1.0 - bar);
    }
    // Row 1: TIMEDELTA - brightness proportional to dt (should be around 1/60)
    else if(row == 1)
    {
        float brightness = clamp(TIMEDELTA * 60.0, 0.0, 1.0);
        c = vec4(0.0, brightness, brightness * 0.5, 1.0);
    }
    // Row 2: PROGRESS - vertical fill (0 to 1 over the process lifetime)
    else if(row == 2)
    {
        float fill = step(cellUV.x, PROGRESS);
        c = vec4(fill * 0.2, fill, fill * 0.5, 1.0);
    }
    // Row 3: FRAMEINDEX - cycling hue
    else if(row == 3)
    {
        float hue = fract(float(FRAMEINDEX) / 120.0);
        c = vec4(
            0.5 + 0.5 * sin(hue * 6.28),
            0.5 + 0.5 * sin(hue * 6.28 + 2.09),
            0.5 + 0.5 * sin(hue * 6.28 + 4.19),
            1.0);
    }
    // Row 4: RENDERSIZE - width as red, height as green, normalized to 2048
    else if(row == 4)
    {
        c = vec4(RENDERSIZE.x / 2048.0, RENDERSIZE.y / 2048.0, 0.5, 1.0);
    }
    // Row 5: DATE - year/month/day/seconds encoded
    else if(row == 5)
    {
        float seconds_frac = fract(DATE.w / 86400.0);
        c = vec4(DATE.y / 12.0, DATE.z / 31.0, seconds_frac, 1.0);
    }

    gl_FragColor = c;
}

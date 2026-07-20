/*{
  "DESCRIPTION": "Tests ISF long input in numeric mode (MIN/MAX without VALUES/LABELS). Should produce an IntSpinBox in the UI, not a ComboBox. Draws N horizontal bars where N = inCount. If the long numeric mode works, the bar count matches the spinner value. Also tests that the int uniform is correctly passed.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-INPUTS"],
  "INPUTS": [
    { "NAME": "inCount", "TYPE": "long", "DEFAULT": 4, "MIN": 1, "MAX": 16 }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;
    int row = int(floor(uv.y * float(inCount)));
    float frac = fract(uv.y * float(inCount));

    // Alternating colored bars
    float hue = float(row) / float(inCount);
    vec3 col = vec3(
        0.5 + 0.5 * sin(hue * 6.28),
        0.5 + 0.5 * sin(hue * 6.28 + 2.09),
        0.5 + 0.5 * sin(hue * 6.28 + 4.19)
    );

    // Gap between bars
    float mask = step(0.05, frac) * step(frac, 0.95);

    gl_FragColor = vec4(col * mask, 1.0);
}

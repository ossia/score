/*{
  "DESCRIPTION": "Uses identifiers that are legal GLSL but reserved by a target language, where SPIRV-Cross does not rename them. Metal: 'quad', 'plus' and 'array' as function names, 'operator' as an input name, 'new', 'or', 'char' and 'throw' as locals or functions (seen in gs_50912, WeirdBalls, gs_48285, Fireworks.vs, gs_40217.4, GlassWalls). Direct3D: 'lerp', 'frac' and 'saturate' as locals next to the mix(), fract() and clamp() SPIRV-Cross spells with those names (classic_pbr_openpbr, gs_50520, Relaxing_Sine). The engine renames them per target. Used by GfxMslReservedNames: renders red on every backend.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST"],
  "INPUTS": [ { "NAME": "operator", "TYPE": "float", "DEFAULT": 1.0 } ]
}*/
vec4 quad(float v)
{
    return vec4(v, 0.0, 0.0, 1.0);
}
float plus(float v) { return v; }
float array(float v) { return v; }
float throw(float v) { return v; }

void main()
{
    float new = plus(operator);
    float or = array(new);
    float char = throw(or);
    float lerp = mix(0.0, char, 1.0);
    float frac = fract(0.25) * 4.0 * lerp;
    float saturate = clamp(frac, 0.0, 1.0);
    isf_FragColor = quad(saturate);
}

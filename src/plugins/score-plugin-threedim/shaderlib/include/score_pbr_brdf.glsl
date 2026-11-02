// score_pbr_brdf.glsl — microfacet BRDF terms for score's scene presets.
//
// One definition of each term:
//   D  GGX / Trowbridge-Reitz (isotropic and anisotropic)
//   V  height-correlated Smith-GGX visibility, V = G / (4 NoL NoV)
//      (Heitz 2014; the glTF sample viewer and Filament form)
//   F  Schlick with an F90
// plus Lambert diffuse and the sheen (Charlie) lobe. `alpha` is always the
// GGX alpha = perceptual roughness².
//
// score_brdf_eval / score_brdf_fake_env keep the interface classic_pbr and
// deferred_lighting were written against.

#ifndef SCORE_PBR_BRDF_GLSL
#define SCORE_PBR_BRDF_GLSL

const float SCORE_PI = 3.14159265358979323846;

float score_D_GGX(float NoH, float alpha)
{
  float a2 = alpha * alpha;
  float d  = NoH * NoH * (a2 - 1.0) + 1.0;
  return a2 / (SCORE_PI * d * d);
}

float score_V_SmithGGXCorrelated(float NoV, float NoL, float alpha)
{
  float a2 = alpha * alpha;
  float gv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
  float gl = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
  float d = gv + gl;
  return d > 0.0 ? 0.5 / d : 0.0;
}

// Anisotropic GGX (KHR_materials_anisotropy, Burley 2012). at / ab are the
// alphas along the tangent T and the bitangent B.
float score_D_GGX_aniso(float NoH, float ToH, float BoH, float at, float ab)
{
  float a2 = at * ab;
  vec3 f = vec3(ab * ToH, at * BoH, a2 * NoH);
  float w2 = a2 / max(dot(f, f), 1e-20);
  return a2 * w2 * w2 / SCORE_PI;
}

float score_V_SmithGGXCorrelated_aniso(float NoV, float NoL, float ToV, float BoV,
                                       float ToL, float BoL, float at, float ab)
{
  float gv = NoL * length(vec3(at * ToV, ab * BoV, NoV));
  float gl = NoV * length(vec3(at * ToL, ab * BoL, NoL));
  return 0.5 / max(gv + gl, 1e-8);
}

vec3 score_F_Schlick(vec3 f0, vec3 f90, float VoH)
{
  float x = clamp(1.0 - VoH, 0.0, 1.0);
  float x2 = x * x;
  return f0 + (f90 - f0) * (x2 * x2 * x);
}

float score_F_Schlick(float f0, float f90, float VoH)
{
  float x = clamp(1.0 - VoH, 0.0, 1.0);
  float x2 = x * x;
  return f0 + (f90 - f0) * (x2 * x2 * x);
}

vec3 score_F_Schlick(vec3 f0, float VoH)
{
  return score_F_Schlick(f0, vec3(1.0), VoH);
}

// Dielectric F0 from an IOR (KHR_materials_ior; 1.5 gives 0.04).
float score_f0_from_ior(float ior)
{
  float r = (ior - 1.0) / (ior + 1.0);
  return r * r;
}

vec3 score_lambert(vec3 albedo)
{
  return albedo * (1.0 / SCORE_PI);
}

// Sheen (KHR_materials_sheen): Charlie distribution (Estevez & Kulla 2017)
// with Neubelt's visibility, as in the glTF sample viewer and Filament.
float score_D_Charlie(float sheenRoughness, float NoH)
{
  float a = max(sheenRoughness * sheenRoughness, 1e-4);
  float invA = 1.0 / a;
  float sin2 = max(1.0 - NoH * NoH, 0.0078125);
  return (2.0 + invA) * pow(sin2, invA * 0.5) / (2.0 * SCORE_PI);
}

float score_V_Neubelt(float NoV, float NoL)
{
  return clamp(1.0 / (4.0 * (NoL + NoV - NoL * NoV)), 0.0, 1.0);
}

// Directional albedo of the Charlie lobe. A rough analytic stand-in for the
// sample viewer's sheen E LUT (grows with roughness, peaks at grazing). Drives
// the albedo scaling of the layers below, 1 - max(sheenColor) * E, and the
// image-based sheen.
float score_sheen_albedo(float NoV, float sheenRoughness)
{
  float r = clamp(sheenRoughness, 0.0, 1.0);
  float c = 1.0 - NoV;
  return clamp(r * (0.11 + 0.36 * c * c * c) + 0.035 * r * r, 0.0, 1.0);
}

// ─── Older interface (deferred_lighting) ─────────────────────────────
struct ScoreBRDFInputs {
  vec3  N;          // world-space surface normal
  vec3  V;          // world-space view direction (surface→camera)
  vec3  base;       // baseColor × any per-vertex / per-instance tints
  float metallic;   // [0, 1]
  float roughness;  // perceptual; clamp the low end (~0.04) before passing in
};

struct ScoreBRDFEval {
  vec3 diffuse;     // Lambertian, energy conservation applied
  vec3 specular;    // D · V · F (V already holds the 1 / (4 NoV NoL))
};

ScoreBRDFEval score_brdf_eval(in ScoreBRDFInputs s, vec3 Ldir)
{
  ScoreBRDFEval o;
  vec3 F0 = mix(vec3(0.04), s.base, s.metallic);
  vec3 H = normalize(Ldir + s.V);
  float NoV = max(dot(s.N, s.V), 1e-4);
  float NoL = max(dot(s.N, Ldir), 0.0);
  float NoH = max(dot(s.N, H), 0.0);
  float VoH = max(dot(s.V, H), 0.0);
  float alpha = max(s.roughness * s.roughness, 1e-4);
  vec3 F = score_F_Schlick(F0, VoH);
  o.specular = score_D_GGX(NoH, alpha) * score_V_SmithGGXCorrelated(NoV, NoL, alpha) * F;
  o.diffuse  = (vec3(1.0) - F) * (1.0 - s.metallic) * score_lambert(s.base);
  return o;
}

// Cheap "fake env" reflection for presets without image-based lighting:
// gives metals a visible baseline. Apply before the light loop.
vec3 score_brdf_fake_env(in ScoreBRDFInputs s)
{
  float NoV = max(dot(s.N, s.V), 1e-4);
  vec3 F0 = mix(vec3(0.04), s.base, s.metallic);
  return score_F_Schlick(F0, NoV) * s.base * 0.25;
}

#endif

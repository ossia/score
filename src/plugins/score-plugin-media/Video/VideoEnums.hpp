#pragma once
namespace Video
{
enum OutputFormat
{
  SDR,         // Default, tonemap to SDR
  Passthrough, // Passthrough HDR content directly
  Linear,      // Convert to linear space
  Normalized   // Linear normalized by peak luminance
};

enum Tonemap
{
  Clamp,
  BT_2390,
  BT_2446,
  Reinhard,
  Hable,
  ACES2,
  AgX,
  PBR_Neutral,
  Auto         // Selects best tonemapper based on content transfer function
};
}

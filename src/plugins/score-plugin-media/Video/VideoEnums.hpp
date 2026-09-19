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

/**
 * @brief How a source delivers the two fields of an interlaced picture.
 *
 * NDI delivers all three: progressive frames, woven frames (its "interleaved"
 * type, both fields already in one full-height buffer), and individual fields
 * at twice the frame rate -- which is what asking for 16-bit gets you, whatever
 * allow_video_fields is set to. Capture cards do the same for the interlaced
 * SDI standards.
 *
 * Only Fields needs anything of the GPU: the frames arrive half-height and are
 * uploaded into the two halves of one full-height texture, which score_tc then
 * samples by parity. Woven needs nothing -- it is a frame, combing and all.
 */
enum class Interlacing
{
  None,   ///< progressive frames
  Woven,  ///< one frame carrying both fields, even lines first
  Fields  ///< one field per frame, half height, at twice the rate
};

/// What to do with separately delivered fields. Mirrors the modes in
/// SCORE_GFX_VIDEO_UNIFORMS' score_tc; see it for what each costs.
enum class Deinterlace
{
  Weave,  ///< full vertical resolution, combs on motion
  Bob     ///< half the vertical resolution, no combing, smooth at field rate
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

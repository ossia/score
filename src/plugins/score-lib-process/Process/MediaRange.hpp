#pragma once

namespace Process
{

//! A region of a media file, in seconds of the file's own timeline.
//! Its own header because models report one without needing anything else
//! from the file-operation machinery.
struct MediaRange
{
  double start{};
  double duration{};

  double end() const noexcept { return start + duration; }
};
}

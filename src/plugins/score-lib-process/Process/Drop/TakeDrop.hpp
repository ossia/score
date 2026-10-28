#pragma once
#include <Process/Commands/Properties.hpp>
#include <Process/Drop/ClipMetadata.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>

#include <score/command/Dispatchers/RuntimeDispatcher.hpp>

#include <QByteArray>
#include <QMimeData>
#include <QString>

#include <vector>

namespace Process
{
//! The clip metadata a drop carries next to its files; empty without any.
inline std::vector<clip_file_metadata> clip_metadata_of(const QMimeData& mime)
{
  const auto fmt = QString::fromUtf8(
      clip_metadata_mime.data(), qsizetype(clip_metadata_mime.size()));
  if(!mime.hasFormat(fmt))
    return {};
  const QByteArray data = mime.data(fmt);
  return decode_clip_metadata({data.constData(), std::size_t(data.size())});
}

//! Sets the clip of a loop to play it round, `length` at a time, from the
//! start of its box: in the step that made the clip, so that one undo takes
//! it all back. Its content lies over its first `length`.
inline void
loop_clip(ProcessModel& proc, TimeVal length, score::Dispatcher& disp)
{
  if(length <= TimeVal::zero())
    return;
  if(!proc.loops())
    disp.submit(new SetLoop{proc, true});
  if(proc.loopDuration() != length)
    disp.submit(new SetLoopDuration{proc, length});
}
}

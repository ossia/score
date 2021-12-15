#pragma once

#include <score/model/ObjectRemover.hpp>

namespace Midi
{
class NoteRemover : public score::ObjectRemover
{
  SCORE_CONCRETE("94325561-b73b-4593-a4ef-46c7e2100078")
  bool remove(const Selection& s, const score::DocumentContext& ctx) override;
};
}

#pragma once

#include <score/model/ObjectEditor.hpp>

namespace Midi
{
class NoteEditor : public score::ObjectEditor
{
  SCORE_CONCRETE("94325561-b73b-4593-a4ef-46c7e2100078")

  bool
  copy(JSONReader& r, const Selection& s, const score::DocumentContext& ctx) override;
  bool paste(
      QPoint pos, QObject* focusedObject, const QMimeData& mime,
      const score::DocumentContext& ctx) override;
  bool remove(const Selection& s, const score::DocumentContext& ctx) override;
};
}

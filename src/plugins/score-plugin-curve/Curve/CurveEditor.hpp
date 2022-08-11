#pragma once
#include <score/model/ObjectEditor.hpp>

namespace Curve
{

class CurveEditor : public score::ObjectEditor
{
  SCORE_CONCRETE("d2b20e55-296f-49cc-a1c5-1ba1a1122d07")

  bool
  copy(JSONReader& r, const Selection& s, const score::DocumentContext& ctx) override;
  bool paste(
      QPoint pos, QObject* focusedObject, const QMimeData& mime,
      const score::DocumentContext& ctx) override;
  bool remove(const Selection& s, const score::DocumentContext& ctx) override;
};

}

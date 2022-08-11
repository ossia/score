#pragma once
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/selection/Selection.hpp>

class QMimeData;
namespace score
{
struct DocumentContext;
// TODO do it for curve, c.f. CurvePresenter::removeSelection
struct SCORE_LIB_BASE_EXPORT ObjectEditor : public score::InterfaceBase
{
  SCORE_INTERFACE(ObjectEditor, "12951ea1-ffb0-4f77-8a3a-bf28ccb60a2e")
public:
  virtual ~ObjectEditor() override;

  virtual bool copy(JSONReader& r, const Selection& s, const score::DocumentContext& ctx)
      = 0;
  virtual bool paste(
      QPoint pos, QObject* focusedObject, const QMimeData& mime,
      const score::DocumentContext& ctx)
      = 0;
  virtual bool remove(const Selection& s, const score::DocumentContext& ctx) = 0;
};

class SCORE_LIB_BASE_EXPORT ObjectEditorList final
    : public score::InterfaceList<ObjectEditor>
{
public:
  virtual ~ObjectEditorList() override;
};

}

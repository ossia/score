#pragma once
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/selection/Selection.hpp>

class QMimeData;
namespace score
{
struct DocumentContext;

//! What removing an element that sits in a chain does with the elements it
//! linked. Editors whose elements are never chained ignore it.
enum class RemovalLinks : bool
{
  //! What fed the removed elements feeds what they fed
  Reconnect,
  //! Their cables go with them
  Disconnect
};

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
  //! Every caller says what happens to links, there is no default: Delete and
  //! the object tree's Remove reconnect, Shift+Delete and Cut disconnect.
  virtual bool
  remove(const Selection& s, const score::DocumentContext& ctx, RemovalLinks links)
      = 0;
};

class SCORE_LIB_BASE_EXPORT ObjectEditorList final
    : public score::InterfaceList<ObjectEditor>
{
public:
  virtual ~ObjectEditorList() override;
};

}

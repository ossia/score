#pragma once
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

class Selection;
namespace score
{
class DocumentDelegateModel;
class DocumentDelegateView;
class DocumentPresenter;

class SCORE_LIB_BASE_EXPORT DocumentDelegatePresenter : public QObject
{
  W_OBJECT(DocumentDelegatePresenter)
public:
  DocumentDelegatePresenter(
      DocumentPresenter* parent_presenter,
      const DocumentDelegateModel& model,
      DocumentDelegateView& view);

  virtual ~DocumentDelegatePresenter();

public:
  virtual void setNewSelection(const Selection& old, const Selection& s) = 0;
  W_SLOT(setNewSelection);

protected:
  const DocumentDelegateModel& m_model;
  DocumentDelegateView& m_view;
  DocumentPresenter* m_parentPresenter{};
};
}

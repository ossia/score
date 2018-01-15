#pragma once
#include <QObject>
#include <QString>
#include <score_lib_base_export.h>

class Selection;
namespace score
{
class DocumentDelegateModel;
class DocumentDelegateView;
class DocumentPresenter;

class SCORE_LIB_BASE_EXPORT DocumentDelegatePresenter
    : public QObject
{
  Q_OBJECT
public:
  DocumentDelegatePresenter(
      DocumentPresenter* parent_presenter,
      const DocumentDelegateModel& model,
      DocumentDelegateView& view);

  virtual ~DocumentDelegatePresenter();

public Q_SLOTS:
  virtual void setNewSelection(const Selection& s) = 0;

protected:
  const DocumentDelegateModel& m_model;
  DocumentDelegateView& m_view;
  DocumentPresenter* m_parentPresenter{};
};
}

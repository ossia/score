#pragma once
#include <QObject>
#include <QString>
#include <iscore_lib_base_export.h>

class Selection;
namespace iscore
{
class DocumentDelegateModel;
class DocumentDelegateView;
class DocumentPresenter;

class ISCORE_LIB_BASE_EXPORT DocumentDelegatePresenter
    : public QObject
{
  Q_OBJECT
public:
  DocumentDelegatePresenter(
      DocumentPresenter* parent_presenter,
      const DocumentDelegateModel& model,
      DocumentDelegateView& view);

  virtual ~DocumentDelegatePresenter();

public slots:
  virtual void setNewSelection(const Selection& s) = 0;

protected:
  const DocumentDelegateModel& m_model;
  DocumentDelegateView& m_view;
  DocumentPresenter* m_parentPresenter{};
};
}

#pragma once
#include <QObject>
#include <QString>
#include <iscore_lib_base_export.h>

namespace iscore
{
class DocumentDelegateModel;
class DocumentDelegateView;
class DocumentPresenter;

class ISCORE_LIB_BASE_EXPORT DocumentDelegatePresenter
    : public QObject
{
public:
  DocumentDelegatePresenter(
      DocumentPresenter* parent_presenter,
      const DocumentDelegateModel& model,
      DocumentDelegateView& view);

  virtual ~DocumentDelegatePresenter();

protected:
  const DocumentDelegateModel& m_model;
  DocumentDelegateView& m_view;
  DocumentPresenter* m_parentPresenter{};
};
}

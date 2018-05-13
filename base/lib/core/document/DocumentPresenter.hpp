#pragma once

#include <QObject>
#include <wobjectdefs.h>

class Selection;
namespace score
{
struct DocumentContext;
class DocumentDelegateFactory;
class DocumentDelegatePresenter;
class DocumentModel;
class DocumentView;
/**
 * @brief Interface between the DocumentModel and the DocumentView.
 */
class DocumentPresenter final : public QObject
{
  W_OBJECT(DocumentPresenter)
public:
  DocumentPresenter(
      const score::DocumentContext& ctx,
      DocumentDelegateFactory&,
      const DocumentModel&,
      DocumentView&,
      QObject* parent);

  DocumentDelegatePresenter* presenterDelegate() const
  {
    return m_presenter;
  }

  void setNewSelection(const Selection& s);

  DocumentView& m_view;
  const DocumentModel& m_model;
  DocumentDelegatePresenter* m_presenter{};
};
}

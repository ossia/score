#pragma once

#include <QObject>

namespace iscore
{
class DocumentDelegateFactory;
class DocumentDelegatePresenterInterface;
class DocumentModel;
class DocumentView;

/**
 * @brief Interface between the DocumentModel and the DocumentView.
 */
class DocumentPresenter final : public QObject
{
  Q_OBJECT
public:
  DocumentPresenter(
      DocumentDelegateFactory&,
      const DocumentModel&,
      DocumentView&,
      QObject* parent);

  DocumentDelegatePresenterInterface* presenterDelegate() const
  {
    return m_presenter;
  }

  DocumentView& m_view;
  const DocumentModel& m_model;
  DocumentDelegatePresenterInterface* m_presenter{};
};
}

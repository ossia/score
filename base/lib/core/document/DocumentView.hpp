#pragma once

#include <QWidget>

namespace iscore
{
class Document;
class DocumentDelegateFactory;
class DocumentDelegateView;
class PanelView;

/**
 * @brief The DocumentView class shows a document.
 *
 * It displays a @c{DocumentDelegateViewInterface}, in
 * the central widget.
 */
class DocumentView final : public QObject
{
  Q_OBJECT
public:
  DocumentView(
      DocumentDelegateFactory& viewDelegate,
      const Document& doc,
      QObject* parent);

  DocumentDelegateView& viewDelegate() const
  {
    return *m_view;
  }

  const Document& document() const
  {
    return m_document;
  }

private:
  const Document& m_document;
  DocumentDelegateView* m_view{};
};
}

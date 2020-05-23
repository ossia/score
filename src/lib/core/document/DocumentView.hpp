#pragma once

#include <verdigris>
#include <score_lib_base_export.h>
namespace score
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
class SCORE_LIB_BASE_EXPORT DocumentView final : public QObject
{
  W_OBJECT(DocumentView)
public:
  DocumentView(DocumentDelegateFactory& viewDelegate, const Document& doc, QObject* parent);

  DocumentDelegateView& viewDelegate() const { return *m_view; }

  const Document& document() const { return m_document; }

private:
  const Document& m_document;
  DocumentDelegateView* m_view{};
};
}

W_REGISTER_ARGTYPE(score::DocumentView*)

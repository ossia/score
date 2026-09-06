#pragma once

#include <score_lib_base_export.h>

#include <verdigris>
class QWidget;
namespace score
{
class Document;
class DocumentDelegateFactory;
class DocumentDelegateView;
class CentralViewStack;

/**
 * @brief The DocumentView class shows a document.
 *
 * It displays a @c{DocumentDelegateViewInterface}, in
 * the central widget, as the first view of a @c{CentralViewStack}:
 * plug-ins can push other views (code editors, process UIs...) there.
 */
class SCORE_LIB_BASE_EXPORT DocumentView final : public QObject
{
  W_OBJECT(DocumentView)
public:
  DocumentView(
      DocumentDelegateFactory& viewDelegate, const Document& doc, QObject* parent);
  ~DocumentView() override;

  DocumentDelegateView& viewDelegate() const { return *m_view; }

  const Document& document() const { return m_document; }

  //! The widget put in the main window for this document.
  QWidget* widget() const noexcept;

  //! The navigation bar and the stack of central views of this document.
  CentralViewStack& centralViews() const noexcept { return *m_central; }

private:
  const Document& m_document;
  DocumentDelegateView* m_view{};
  CentralViewStack* m_central{};
};
}

W_REGISTER_ARGTYPE(score::DocumentView*)

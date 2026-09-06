// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateView.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentView.hpp>
#include <core/view/CentralViewStack.hpp>

#include <QWidget>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::DocumentView)
namespace score
{
DocumentView::DocumentView(
    DocumentDelegateFactory& fact, const Document& doc, QObject* parent)
    : QObject{parent}
    , m_document{doc}
    , m_view{fact.makeView(doc.context(), this)}
    , m_central{new CentralViewStack{m_view->getWidget(), tr("Score")}}
{
}

DocumentView::~DocumentView()
{
  // The delegate view manages its widget's lifetime itself
  m_central->releaseMainView();
  delete m_central;
}

QWidget* DocumentView::widget() const noexcept
{
  return m_central;
}
}

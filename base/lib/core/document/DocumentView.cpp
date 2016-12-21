#include <QGridLayout>
#include <QLayout>
#include <core/document/Document.hpp>
#include <core/document/DocumentView.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateView.hpp>

#include <iscore/document/DocumentContext.hpp>

namespace iscore
{
DocumentView::DocumentView(
    DocumentDelegateFactory& fact, const Document& doc, QWidget* parent)
    : QWidget{parent}
    , m_document{doc}
    , m_view{fact.makeView(m_document.context().app, this)}
{
  m_view->setParent(this);
  setObjectName("DocumentView");
  setLayout(new QGridLayout{this});
  layout()->setContentsMargins(0, 0, 0, 0);
  layout()->addWidget(m_view->getWidget());
}
}

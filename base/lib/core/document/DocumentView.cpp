#include <QGridLayout>
#include <QLayout>
#include <core/document/Document.hpp>
#include <core/document/DocumentView.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateView.hpp>

#include <iscore/document/DocumentContext.hpp>
#include <iscore/widgets/MarginLess.hpp>

namespace iscore
{
DocumentView::DocumentView(
    DocumentDelegateFactory& fact,
    const Document& doc,
    QObject* parent)
    : QObject{parent}
    , m_document{doc}
    , m_view{fact.makeView(m_document.context().app, this)}
{
}
}

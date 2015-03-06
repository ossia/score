#include <core/document/DocumentView.hpp>
#include <interface/documentdelegate/DocumentDelegateViewInterface.hpp>
#include <interface/documentdelegate/DocumentDelegateFactoryInterface.hpp>


#include <QGridLayout>
using namespace iscore;

DocumentView::DocumentView(DocumentDelegateFactoryInterface* fact,
                           Document* doc,
                           QWidget* parent) :
    QWidget {parent},
    m_document{doc},
    m_view{fact->makeView(this)}

{
    setObjectName("DocumentView");
    setLayout(new QGridLayout{this});
    layout()->setContentsMargins(0, 0, 0, 0);
    layout()->addWidget(m_view->getWidget());
}

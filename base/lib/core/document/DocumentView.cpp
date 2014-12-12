#include <core/document/DocumentView.hpp>
#include <interface/documentdelegate/DocumentDelegateViewInterface.hpp>

#include <QGridLayout>
using namespace iscore;

DocumentView::DocumentView(QWidget* parent):
	QWidget{parent}
{
	this->setObjectName("DocumentView");
	this->setLayout(new QGridLayout{this});
	this->layout()->setContentsMargins(0,0,0,0);
}

void DocumentView::setViewDelegate(DocumentDelegateViewInterface* view)
{
	if(m_view)
	{
		auto old_view = m_view;
		m_view = view;
		auto olditem = layout()->replaceWidget(old_view->getWidget(), m_view->getWidget());

		delete olditem;
		old_view->deleteLater();
	}
	else
	{
		m_view = view;
		layout()->addWidget(view->getWidget());
	}
}

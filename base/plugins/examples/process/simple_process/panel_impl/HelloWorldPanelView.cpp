#include "HelloWorldPanelView.hpp"
#include "HelloWorldPanelPresenter.hpp"

#include <QLabel>

using namespace iscore;

HelloWorldPanelView::HelloWorldPanelView():
	iscore::PanelViewInterface{nullptr}
{
	this->setObjectName("Hello Small Panel");
}

void HelloWorldPanelView::setPresenter(PanelPresenterInterface* presenter)
{
	m_presenter = static_cast<HelloWorldPanelPresenter*>(presenter);
}

QWidget* HelloWorldPanelView::getWidget()
{
	return new QLabel("Panneau latéral");
}

#include "HelloWorldPanelView.hpp"
#include "HelloWorldPanelPresenter.hpp"

#include <QLabel>

using namespace iscore;

HelloWorldPanelView::HelloWorldPanelView():
	iscore::PanelView{nullptr}
{
	this->setObjectName("Hello Small Panel");
}

void HelloWorldPanelView::setPresenter(PanelPresenter* presenter)
{
	m_presenter = static_cast<HelloWorldPanelPresenter*>(presenter);
}

QWidget* HelloWorldPanelView::getWidget()
{
	return new QLabel("Panneau latéral");
}

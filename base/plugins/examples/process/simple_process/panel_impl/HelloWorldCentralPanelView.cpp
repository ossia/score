#include "HelloWorldCentralPanelView.hpp"
#include "HelloWorldCentralPanelPresenter.hpp"

#include <QLabel>

using namespace iscore;

HelloWorldCentralPanelView::HelloWorldCentralPanelView():
	iscore::PanelView{nullptr}
{

}

void HelloWorldCentralPanelView::setPresenter(PanelPresenter* presenter)
{
	m_presenter = static_cast<HelloWorldCentralPanelPresenter*>(presenter);
}

QWidget* HelloWorldCentralPanelView::getWidget()
{
	return new QLabel("Panneau central");
}

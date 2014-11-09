#include "HelloWorldCentralPanelView.hpp"
#include "HelloWorldCentralPanelPresenter.hpp"

#include <QLabel>

using namespace iscore;

HelloWorldCentralPanelView::HelloWorldCentralPanelView():
	iscore::DocumentDelegateViewInterface{nullptr}
{

}

void HelloWorldCentralPanelView::setPresenter(DocumentDelegatePresenterInterface* presenter)
{
	m_presenter = static_cast<HelloWorldCentralPanelPresenter*>(presenter);
}

QWidget* HelloWorldCentralPanelView::getWidget()
{
	return new QLabel("Panneau central");
}

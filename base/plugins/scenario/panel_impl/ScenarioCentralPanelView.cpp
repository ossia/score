#include "ScenarioCentralPanelView.hpp"
#include "ScenarioCentralPanelPresenter.hpp"
#include "MainWindow.hpp"
#include <QPushButton>
#include <interface/documentdelegate/DocumentDelegateViewInterface.hpp>
#include <interface/documentdelegate/DocumentDelegatePresenterInterface.hpp>


using namespace iscore;

ScenarioCentralPanelView::ScenarioCentralPanelView():
	iscore::DocumentDelegateViewInterface{nullptr}
{

}

void ScenarioCentralPanelView::setPresenter(DocumentDelegatePresenterInterface* presenter)
{
	m_presenter = static_cast<ScenarioCentralPanelPresenter*>(presenter);
}

QWidget* ScenarioCentralPanelView::getWidget()
{
	return new MainWindow;
}

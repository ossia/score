#include "ScenarioCentralPanelView.hpp"
#include "ScenarioCentralPanelPresenter.hpp"
#include "MainWindow.hpp"
#include <QPushButton>
#include <interface/docpanel/DocumentPanelView.hpp>
#include <interface/docpanel/DocumentPanelPresenter.hpp>


using namespace iscore;

ScenarioCentralPanelView::ScenarioCentralPanelView():
	iscore::DocumentPanelView{nullptr}
{

}

void ScenarioCentralPanelView::setPresenter(DocumentPanelPresenter* presenter)
{
	m_presenter = static_cast<ScenarioCentralPanelPresenter*>(presenter);
}

QWidget* ScenarioCentralPanelView::getWidget()
{
    return new MainWindow;
}

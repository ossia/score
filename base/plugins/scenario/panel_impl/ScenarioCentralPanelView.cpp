#include "ScenarioCentralPanelView.hpp"
#include "ScenarioCentralPanelPresenter.hpp"
//#include "boxes/mainbox.hpp"
#include "boxes/mainwindow.hpp"
#include <QPushButton>

using namespace iscore;

ScenarioCentralPanelView::ScenarioCentralPanelView():
	iscore::PanelView{nullptr}
{

}

void ScenarioCentralPanelView::setPresenter(PanelPresenter* presenter)
{
	m_presenter = static_cast<ScenarioCentralPanelPresenter*>(presenter);
}

QWidget* ScenarioCentralPanelView::getWidget()
{
	return new MainWindow;
}

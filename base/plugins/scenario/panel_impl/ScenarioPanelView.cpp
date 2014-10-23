#include "ScenarioPanelView.hpp"
#include "ScenarioPanelPresenter.hpp"

#include <QLabel>

using namespace iscore;

ScenarioPanelView::ScenarioPanelView():
	iscore::PanelView{nullptr}
{
	this->setObjectName("Hello Small Panel");
}

void ScenarioPanelView::setPresenter(PanelPresenter* presenter)
{
	m_presenter = static_cast<ScenarioPanelPresenter*>(presenter);
}

QWidget* ScenarioPanelView::getWidget()
{
	return new QLabel("Roulade latérale");
}

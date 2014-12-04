#include <core/model/Model.hpp>
#include <interface/panel/PanelModelInterface.hpp>

using namespace iscore;

void Model::addPanel(PanelModelInterface* m)
{
	m_panelsModels.insert(m);
	qDebug(Q_FUNC_INFO);
}

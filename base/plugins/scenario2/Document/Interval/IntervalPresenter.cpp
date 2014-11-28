#include "IntervalPresenter.hpp"
#include "IntervalModel.hpp"
#include "IntervalContent/IntervalContentPresenter.hpp"

IntervalPresenter::IntervalPresenter(IntervalModel* model, QObject* parent):
	QNamedObject{parent, "IntervalPresenter"},
	m_model{model}
{
	// Faire un parcours récursif du modèle pour créer des présenteurs adaptés ?
	for(auto& contentModel : m_model->contentModels())
	{
		m_contentModels.push_back(new IntervalContentPresenter(contentModel, this));
	}

}
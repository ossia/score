#include "IntervalPresenter.hpp"
#include "IntervalModel.hpp"
#include "IntervalView.hpp"
#include "IntervalContent/IntervalContentPresenter.hpp"
#include "IntervalContent/IntervalContentView.hpp"

#include <QDebug>

IntervalPresenter::IntervalPresenter(IntervalModel* model,
									 IntervalView* view,
									 QObject* parent):
	QNamedObject{parent, "IntervalPresenter"},
	m_model{model},
	m_view{view}
{
	qDebug(Q_FUNC_INFO);
	view->m_rect.setWidth(model->m_width);
	view->m_rect.setHeight(model->m_height);
	qDebug() << "IntervalPresenter: setting width, height" << model->m_width << model->m_height;
	qDebug("Adding Interval Content");

	// Todo : ou s'enregistre le contentView? Dans le présenteur ?
	// Le présenteur parent va créer les vues correspondant aux présenteurs enfants
	auto contentView = new IntervalContentView{view};

	// Cas par défaut
	m_contentPresenters.push_back(new IntervalContentPresenter{model->contentModel(0),
															   contentView,
															   this});
}



/*
	// Parcours récursif du modèle pour créer des présenteurs adaptés
	for(auto& contentModel : m_model->contentModels())
	{
		qDebug("Adding Interval Content");

		// Todo : ou s'enregistre le contentView? Dans le présenteur ?
		// Le présenteur parent va créer les vues correspondant aux présenteurs enfants
		auto contentView = new IntervalContentView{view};

		m_contentPresenters.push_back(new IntervalContentPresenter{contentModel,
																   contentView,
																   this});
	}
*/
#include "IntervalPresenter.hpp"
#include "IntervalModel.hpp"
#include "IntervalView.hpp"
#include "IntervalContent/IntervalContentPresenter.hpp"
#include "IntervalContent/IntervalContentView.hpp"

#include <core/presenter/command/SerializableCommand.hpp>
#include <QDebug>

IntervalPresenter::IntervalPresenter(IntervalModel* model,
									 IntervalView* view,
									 QObject* parent):
	QNamedObject{parent, "IntervalPresenter"},
	m_model{model},
	m_view{view}
{
	view->m_rect.setWidth(model->m_width);
	view->m_rect.setHeight(model->m_height);

	// Le contentView est child de IntervalView (au sens Qt) mais est accessible via son présenteur.
	// Le présenteur parent va créer les vues correspondant aux présenteurs enfants
	auto contentView = new IntervalContentView{view};

	// Cas par défaut
	auto interval_presenter = new IntervalContentPresenter{model->contentModel(0),
														   contentView,
														   this};

	m_contentPresenters.push_back(interval_presenter);

	connect(this, SIGNAL(submitCommand(iscore::SerializableCommand*)),
			parent, SIGNAL(submitCommand(iscore::SerializableCommand*)));
}

IntervalPresenter::~IntervalPresenter()
{
	m_view->deleteLater();
}

int IntervalPresenter::id() const
{
	return m_model->id();
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
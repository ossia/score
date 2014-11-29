#include "EventPresenter.hpp"
#include "EventModel.hpp"
#include "EventView.hpp"


EventPresenter::EventPresenter(EventModel* model,
							   EventView* view,
							   QObject* parent):
	QNamedObject{parent, "EventPresenter"},
	m_model{model},
	m_view{view}
{
}

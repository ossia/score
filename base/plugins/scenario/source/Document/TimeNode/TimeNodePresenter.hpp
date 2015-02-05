#pragma once
#include <tools/NamedObject.hpp>
#include <tools/SettableIdentifier.hpp>

#include <QObject>

class TimeNodeView;
class TimeNodeModel;
struct EventData;

class TimeNodePresenter :  public NamedObject
{
	Q_OBJECT
	public:
		explicit TimeNodePresenter(TimeNodeModel* model, TimeNodeView* view, QObject *parent);
		~TimeNodePresenter();

	id_type<TimeNodeModel> id() const;
	int32_t id_val() const
	{ return *id().val(); }

	TimeNodeModel* model();
	TimeNodeView* view();

	signals:
        void timeNodeReleased(EventData);

	public slots:
        void on_timeNodeReleased(QPointF);

	private:
		TimeNodeModel* m_model{};
		TimeNodeView* m_view{};
};

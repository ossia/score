#pragma once
#include <tools/NamedObject.hpp>
#include <tools/SettableIdentifierAlternative.hpp>

#include <QObject>

class TimeNodeView;
class TimeNodeModel;

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

	public slots:

	private:
		TimeNodeModel* m_model{};
		TimeNodeView* m_view{};
};

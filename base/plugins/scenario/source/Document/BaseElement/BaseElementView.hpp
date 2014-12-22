#pragma once
#include <tools/NamedObject.hpp>
#include <interface/documentdelegate/DocumentDelegateViewInterface.hpp>

class QGraphicsScene;
class QGraphicsView;
class TemporalConstraintView;
class BaseElementView : public iscore::DocumentDelegateViewInterface
{
	Q_OBJECT

	public:
		BaseElementView(QObject* parent);
		virtual ~BaseElementView() = default;

		virtual QWidget*getWidget();

		TemporalConstraintView* constraintView()
		{ return m_constraint; }

		void update();

		QGraphicsScene* scene()
		{ return m_scene; }
	private:
		QGraphicsScene* m_scene{};
		QGraphicsView* m_view{};
		QGraphicsObject* m_baseObject{};
		TemporalConstraintView* m_constraint{};

};


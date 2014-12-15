#pragma once
#include <tools/NamedObject.hpp>
#include <interface/documentdelegate/DocumentDelegateViewInterface.hpp>

class QGraphicsScene;
class QGraphicsView;
class ConstraintView;
class BaseElementView : public iscore::DocumentDelegateViewInterface
{
	Q_OBJECT

	public:
		BaseElementView(QObject* parent);
		virtual ~BaseElementView() = default;

		virtual QWidget*getWidget();

		ConstraintView* constraintView()
		{ return m_constraint; }

		void update();

	private:
		QGraphicsScene* m_scene{};
		QGraphicsView* m_view{};
		QGraphicsObject* m_baseObject{};
		ConstraintView* m_constraint{};

};


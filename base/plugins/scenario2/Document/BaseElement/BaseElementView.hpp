#pragma once
#include <QNamedObject>
#include <interface/documentdelegate/DocumentDelegateViewInterface.hpp>
class QGraphicsScene;
class QGraphicsView;
class IntervalView;
class BaseElementView : public iscore::DocumentDelegateViewInterface
{
	Q_OBJECT

	public:
		BaseElementView(QObject* parent);
		virtual ~BaseElementView() = default;

		virtual QWidget*getWidget();

		IntervalView* intervalView()
		{ return m_interval; }

	private:
		QGraphicsScene* m_scene{};
		QGraphicsView* m_view{};
		QGraphicsObject* m_baseObject{};
		IntervalView* m_interval{};

};


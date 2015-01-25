#pragma once
#include <tools/NamedObject.hpp>
#include <interface/documentdelegate/DocumentDelegateViewInterface.hpp>

class QGraphicsScene;
class QGraphicsView;
class TemporalConstraintView;
class AddressBar;
class BaseElementView : public iscore::DocumentDelegateViewInterface
{
	Q_OBJECT

	public:
		BaseElementView(QObject* parent);
		virtual ~BaseElementView() = default;

		virtual QWidget*getWidget();

		QGraphicsObject* baseObject()
		{ return m_baseObject; }

		void update();

		QGraphicsScene* scene()
		{ return m_scene; }

		QGraphicsView* view()
		{ return m_view; }

		AddressBar* addressBar()
		{ return m_addressBar; }

	signals:
		void horizontalZoomChanged(int newZoom);
		void verticalZoomChanged(int newZoom);

	private:
		QWidget* m_widget{};
		QGraphicsScene* m_scene{};
		QGraphicsView* m_view{};
		QGraphicsObject* m_baseObject{};
		TemporalConstraintView* m_constraint{};
		AddressBar* m_addressBar{};
};


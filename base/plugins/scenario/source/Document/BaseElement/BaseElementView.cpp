#include "BaseElementView.hpp"

#include "Document/Constraint/Temporal/TemporalConstraintView.hpp"

#include <QLabel>
#include <QGridLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include "Widgets/AddressBar.hpp"
class GrapicsProxyObject : public QGraphicsObject
{
	public:
		using QGraphicsObject::QGraphicsObject;
		public:
		virtual QRectF boundingRect() const
		{
			return QRectF{};
		}
		virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
		{
		}
};




#include <QDebug>
BaseElementView::BaseElementView(QObject* parent):
	iscore::DocumentDelegateViewInterface{parent},
	m_scene{new QGraphicsScene{this}},
	m_view{new QGraphicsView{m_scene}},
	m_baseObject{new GrapicsProxyObject{}},
	m_addressBar{new AddressBar{nullptr}}
{
	// Configuration
	m_view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
	m_view->setAlignment(Qt::AlignTop | Qt::AlignLeft);

	// Address bar
	// TODO set length = length of the view.
	auto addressBarGraphicsWidget = new QGraphicsProxyWidget;
	addressBarGraphicsWidget->setWidget(m_addressBar);

	// Positions
	addressBarGraphicsWidget->setPos(0, 0);
	m_baseObject->setPos(0, 50);

	m_scene->addItem(addressBarGraphicsWidget);
	m_scene->addItem(m_baseObject);
}

QWidget* BaseElementView::getWidget()
{
	return m_view;
}

void BaseElementView::update()
{
	m_scene->update();
}

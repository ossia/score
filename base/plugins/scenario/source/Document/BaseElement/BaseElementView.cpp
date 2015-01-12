#include "BaseElementView.hpp"

#include "Document/Constraint/Temporal/TemporalConstraintView.hpp"

#include <QLabel>
#include <QGridLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>

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
	m_baseObject{new GrapicsProxyObject{}}
{
//	m_scene->setSceneRect(0, 0, 500, 200);
	m_view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

	m_view->setAlignment(Qt::AlignTop | Qt::AlignLeft);
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

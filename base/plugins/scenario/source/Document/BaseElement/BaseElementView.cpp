#include "BaseElementView.hpp"

#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"

#include <QLabel>
#include <QGridLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QSlider>
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
	m_widget{new QWidget{}},
	m_scene{new QGraphicsScene{this}},
	m_view{new QGraphicsView{m_scene}},
	m_baseObject{new GrapicsProxyObject{}},
	m_addressBar{new AddressBar{nullptr}}
{
	// Configuration
	m_view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
	m_view->setAlignment(Qt::AlignTop | Qt::AlignLeft);

	// Zoom
	QWidget* m_zoomWidget = new QWidget;
	QHBoxLayout* zoomLayout = new QHBoxLayout;
	auto positionSlider = new QSlider{Qt::Horizontal};
	positionSlider->setMinimum(0);
	// TODO quelles sont les unités ?
	positionSlider->setMaximum(50);
	auto horizontalZoomSlider = new QSlider{Qt::Horizontal};
	horizontalZoomSlider->setMinimum(20);
	horizontalZoomSlider->setMaximum(80);
	horizontalZoomSlider->setValue(50);

	connect(horizontalZoomSlider, SIGNAL(sliderMoved(int)),
			this, SIGNAL(horizontalZoomChanged(int)));

	zoomLayout->addWidget(positionSlider);
	zoomLayout->addWidget(horizontalZoomSlider);
	m_zoomWidget->setLayout(zoomLayout);

	m_scene->addItem(m_baseObject);

	auto lay = new QVBoxLayout;
	m_widget->setLayout(lay);
	lay->addWidget(m_addressBar);
	lay->addWidget(m_view);
	lay->addWidget(m_zoomWidget);
}

QWidget* BaseElementView::getWidget()
{
	return m_widget;
}

void BaseElementView::update()
{
	m_scene->update();
}

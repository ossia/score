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


void ScoreGraphicsView::resizeEvent(QResizeEvent* ev)
{
	QGraphicsView::resizeEvent(ev);
	emit widthChanged(width());
}


#include <QDebug>
BaseElementView::BaseElementView(QObject* parent):
	iscore::DocumentDelegateViewInterface{parent},
	m_widget{new QWidget{}},
	m_scene{new QGraphicsScene{this}},
	m_view{new ScoreGraphicsView{m_scene}},
	m_baseObject{new GrapicsProxyObject{}},
	m_addressBar{new AddressBar{nullptr}}
{
	// Configuration
	m_view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
	m_view->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	// Zoom
	QWidget* m_zoomWidget = new QWidget;
	QHBoxLayout* zoomLayout = new QHBoxLayout;
	m_positionSlider = new QSlider{Qt::Horizontal};

	// TODO quelles sont les unités ?
	m_positionSlider->setMinimum(0);
	m_positionSlider->setMaximum(100);
	connect(m_positionSlider, SIGNAL(sliderMoved(int)),
			this, SLOT(on_positionSliderReleased(int)));


	m_zoomSlider = new QSlider{Qt::Horizontal};
	m_zoomSlider->setMinimum(10);
	m_zoomSlider->setMaximum(500);
	m_zoomSlider->setValue(50);

	connect(m_zoomSlider, SIGNAL(sliderMoved(int)),
			this, SIGNAL(horizontalZoomChanged(int)));

	zoomLayout->addWidget(m_positionSlider);
	zoomLayout->addWidget(m_zoomSlider);
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

void BaseElementView::on_positionSliderReleased(int val)
{
	// TODO check that it really changed.
	emit positionSliderChanged(val);
}


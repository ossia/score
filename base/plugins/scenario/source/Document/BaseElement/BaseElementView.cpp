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

	// Address bar
	// TODO set length = length of the view.
	//auto addressBarGraphicsWidget = new QGraphicsProxyWidget;
	//addressBarGraphicsWidget->setWidget(m_addressBar);

	// Zoom
	QWidget* m_zoomWidget = new QWidget;
	QHBoxLayout* zoomLayout = new QHBoxLayout;
	auto verticalZoomSlider = new QSlider{Qt::Horizontal};
	auto horizontalZoomSlider = new QSlider{Qt::Horizontal};

	connect(horizontalZoomSlider, SIGNAL(sliderMoved(int)),
			this, SIGNAL(horizontalZoomChanged(int)));

	zoomLayout->addWidget(verticalZoomSlider);
	zoomLayout->addWidget(horizontalZoomSlider);
	m_zoomWidget->setLayout(zoomLayout);

	//auto zoomBarGraphicsWidget = new QGraphicsProxyWidget;
	//zoomBarGraphicsWidget->setWidget(m_zoomWidget);

	// Positions
	//addressBarGraphicsWidget->setPos(0, 0);
	//m_baseObject->setPos(0, 0);

	//m_scene->addItem(addressBarGraphicsWidget);
	m_scene->addItem(m_baseObject);
	//m_scene->addItem(zoomBarGraphicsWidget);


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

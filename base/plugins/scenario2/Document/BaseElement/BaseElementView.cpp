#include "BaseElementView.hpp"
#include <QLabel>
#include <QGridLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include "BaseElementPresenter.hpp"

BaseElementView::BaseElementView(QObject* parent):
	iscore::DocumentDelegateViewInterface{parent},
	m_scene{new QGraphicsScene{this}},
	m_view{new QGraphicsView{m_scene}}
{
	m_view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
}

void BaseElementView::setPresenter(iscore::DocumentDelegatePresenterInterface* presenter)
{
	m_presenter = static_cast<BaseElementPresenter*>(presenter);
}

QWidget* BaseElementView::getWidget()
{
	return m_view;
}
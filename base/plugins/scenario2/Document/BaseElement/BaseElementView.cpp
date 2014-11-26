#include "BaseElementView.hpp"
#include <QLabel>

void BaseElementView::setPresenter(iscore::DocumentDelegatePresenterInterface* presenter)
{
}

QWidget* BaseElementView::getWidget()
{
	return new QLabel("la tartiflette");
}
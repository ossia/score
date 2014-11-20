#include "BaseElementView.hpp"


void BaseElementView::setPresenter(iscore::DocumentDelegatePresenterInterface* presenter)
{
}

QWidget*BaseElementView::getWidget()
{
	return new QWidget;
}
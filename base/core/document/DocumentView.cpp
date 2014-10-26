#include <core/document/DocumentView.hpp>
#include <QGridLayout>
using namespace iscore;

DocumentView::DocumentView(QWidget* parent):
	QWidget{parent}
{
	this->setLayout(new QGridLayout{this});
	this->layout()->setContentsMargins(0,0,0,0);
}

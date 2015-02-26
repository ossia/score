#include "ReorderWidget.hpp"
#include <QListWidget>

ReorderWidget::ReorderWidget (QWidget* parent) :
    QWidget (parent)
{
}

ReorderWidget::ReorderWidget (std::vector<QWidget*> widgets, QWidget* parent) :
    QWidget (parent)
{

}

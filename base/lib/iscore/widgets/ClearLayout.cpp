#include "ClearLayout.hpp"
#include <QLayout>
#include <QWidget>

void clearLayout(QLayout *layout)
{
    QLayoutItem *child{};
    while ((child = layout->takeAt(0)) != nullptr)
    {
        if(child->layout() != nullptr)
            clearLayout( child->layout() );
        else if(child->widget() != nullptr)
            delete child->widget();

        delete child;
    }
}

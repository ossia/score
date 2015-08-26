#include "ClearLayout.hpp"
#include <QLayout>
#include <QWidget>

void clearLayout(QLayout *layout)
{
    QLayoutItem *child{};
    while ((child = layout->takeAt(0)) != 0)
    {
        if(child->layout() != 0)
            clearLayout( child->layout() );
        else if(child->widget() != 0)
            delete child->widget();

        delete child;
    }
}

#include <qapplication.h>
#include <qlayout.h>
#include <qlayoutitem.h>
#include <qwidget.h>

#include "ClearLayout.hpp"

void iscore::clearLayout(QLayout *layout)
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

void iscore::setCursor(Qt::CursorShape c)
{
    if(qApp)
    {
        if(auto w = qApp->activeWindow())
        {
            w->setCursor(c);
        }
    }
}

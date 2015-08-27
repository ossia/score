#pragma once
#include <QToolBar>

namespace iscore
{
/**
 * @brief The OrderedToolbar struct
 *
 * A toolbar that is meant to have an order with regard to other toolbars.
 */
struct OrderedToolbar
{
    OrderedToolbar(int n, QToolBar* toolbar):
        position{n},
        bar{toolbar}
    {

    }

    int position{}; // 0 = left.
    QToolBar* bar{};
};


inline bool operator<(const OrderedToolbar &lhs, const OrderedToolbar &rhs)
{
    return lhs.position < rhs.position;
}
}

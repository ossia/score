#pragma once
#include <QToolBar>

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

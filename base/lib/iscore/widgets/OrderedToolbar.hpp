#pragma once
#include <QToolBar>

struct OrderedToolbar
{
    unsigned int position{}; // 0 = left.
    QToolBar* bar{};
};


inline bool operator<(const OrderedToolbar &lhs, const OrderedToolbar &rhs)
{
    return lhs.position < rhs.position;
}

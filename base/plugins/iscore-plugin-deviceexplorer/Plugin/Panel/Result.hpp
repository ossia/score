#pragma once

#include <QModelIndex>

namespace DeviceExplorer
{
    struct Result
    {
        Result(bool ok_ = true, const QModelIndex& index_ = QModelIndex())
            : ok(ok_), index(index_)
        {}

        explicit Result(const QModelIndex& index_)
            : ok(true), index(index_)
        {}

        //Type-cast operators
        operator bool() const
        {
            return ok;
        }
        operator QModelIndex() const
        {
            return index;
        }

        bool ok;
        QModelIndex index;
    };
}

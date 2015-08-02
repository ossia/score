#pragma once
#include <vector>
#include <utility>
#include <QByteArray>

namespace iscore
{
struct DocumentBackups
{
        static bool canRestoreDocuments();

        // First is the data, second is the commands.
        static std::vector<std::pair<QByteArray, QByteArray>> restorableDocuments();

        static void clear();
};

}

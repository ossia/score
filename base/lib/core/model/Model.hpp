#pragma once
#include <QObject>
#include <set>

namespace iscore
{
    class PanelModelInterface;
    /**
     * @brief The Model class holds the models of the panels.
     *
     */

    // TODO maybe this should hold some global data such as :
    // - the plug-ins & plugin controls?
    // - the settings ?
    class Model : public QObject
    {
        public:
            using QObject::QObject;
    };
}

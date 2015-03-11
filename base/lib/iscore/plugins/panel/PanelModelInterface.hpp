#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/selection/Selection.hpp>
#include <QJsonObject>

namespace iscore
{
    class PanelPresenterInterface;
    class PanelModelInterface : public NamedObject
    {
            Q_OBJECT
        public:
            using NamedObject::NamedObject;
            virtual ~PanelModelInterface() = default;

            virtual QJsonObject toJson() { return QJsonObject{}; }
            virtual QByteArray toByteArray() { return QByteArray{}; }

        public slots:
            virtual void setNewSelection(const Selection&) { }
    };
}

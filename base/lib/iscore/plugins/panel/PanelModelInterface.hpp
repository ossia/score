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

            // An identifier that is to be shared between the panel presenter and this.
            virtual int panelId() const = 0;

            virtual void serialize(const VisitorVariant&) const {}

        public slots:
            virtual void setNewSelection(const Selection&) { }
    };
}

#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/selection/Selection.hpp>
#include <QJsonObject>

namespace iscore
{
    class PanelPresenter;
    class PanelModel : public NamedObject
    {
            Q_OBJECT
        public:
            using NamedObject::NamedObject;

            virtual int panelId() const = 0;
            virtual void serialize(const VisitorVariant&) const;

        public slots:
            virtual void setNewSelection(const Selection&);
    };
}

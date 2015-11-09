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
            virtual ~PanelModel();

            virtual int panelId() const = 0;

        public slots:
            virtual void setNewSelection(const Selection&);
    };
}

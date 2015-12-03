#pragma once
#include <iscore/selection/Selection.hpp>
#include <iscore/tools/NamedObject.hpp>

namespace iscore
{
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

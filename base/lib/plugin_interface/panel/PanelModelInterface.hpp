#pragma once
#include <tools/NamedObject.hpp>
#include <public_interface/selection/Selection.hpp>
namespace iscore
{
    class PanelPresenterInterface;
    class PanelModelInterface : public NamedObject
    {
            Q_OBJECT
        public:
            using NamedObject::NamedObject;
            virtual ~PanelModelInterface() = default;

        public slots:
            virtual void setNewSelection(const Selection&) { }
    };
}

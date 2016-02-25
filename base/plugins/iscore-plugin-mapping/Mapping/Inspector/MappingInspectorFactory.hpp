#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <Mapping/Inspector/MappingInspectorWidget.hpp>
#include <Mapping/MappingModel.hpp>

namespace Mapping
{
class MappingInspectorFactory final : public Process::InspectorWidgetDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("14b3dc85-6152-4526-8d61-6b038ec5d676")
    public:
        MappingInspectorFactory() = default;

    private:
        Process::InspectorWidgetDelegate* make(
                const Process::ProcessModel& process,
                const iscore::DocumentContext& doc,
                QWidget* parent) const override
        {
            return new MappingInspectorWidget{
                static_cast<const ProcessModel&>(process), doc, parent};

        }

        bool matches(const Process::ProcessModel& process) const override
        {
            return dynamic_cast<const ProcessModel*>(&process);
        }

};
}

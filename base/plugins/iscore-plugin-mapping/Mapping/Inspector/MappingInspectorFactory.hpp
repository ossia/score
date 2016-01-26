#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <Mapping/Inspector/MappingInspectorWidget.hpp>
#include <Mapping/MappingModel.hpp>

namespace Mapping
{
class MappingInspectorFactory final : public ProcessInspectorWidgetDelegateFactory
{
    public:
        MappingInspectorFactory() = default;

    private:
        ProcessInspectorWidgetDelegate* make(
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

#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <QString>
#include <vector>

#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/tools/std/Pointer.hpp>
#include <iscore/tools/std/OwningVector.hpp>
class IdentifiedObjectAbstract;
class QWidget;

namespace iscore
{
}

class InspectorWidgetBase;

class InspectorWidgetList final : public iscore::FactoryListInterface
{
    public:
        static const iscore::FactoryBaseKey& staticFactoryKey() {
            return InspectorWidgetFactory::staticFactoryKey();
        }

        iscore::FactoryBaseKey name() const final override {
            return InspectorWidgetFactory::staticFactoryKey();
        }
        void insert(std::unique_ptr<iscore::FactoryInterfaceBase> e) final override
        {
            if(auto pf = dynamic_unique_ptr_cast<InspectorWidgetFactory>(std::move(e)))
                m_list.emplace_back(std::move(pf));
        }
        const auto& list() const
        { return m_list; }

        InspectorWidgetBase* makeInspectorWidget(
                const QString& name,
                const IdentifiedObjectAbstract& model,
                QWidget* parent) const;

    private:
        OwningVector<InspectorWidgetFactory> m_list;
};

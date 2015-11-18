#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <vector>
namespace iscore
{
class FactoryInterfaceBase;
}

class InspectorWidgetBase;
class InspectorWidgetFactory;
class InspectorWidgetList final : public iscore::FactoryListInterface
{
    public:
        static const iscore::FactoryBaseKey& staticFactoryKey() {
            return InspectorWidgetFactory::staticFactoryKey();
        }

        iscore::FactoryBaseKey name() const final override {
            return InspectorWidgetFactory::staticFactoryKey();
        }
        void insert(iscore::FactoryInterfaceBase* e) final override
        {
            if(auto pf = dynamic_cast<InspectorWidgetFactory*>(e))
                m_list.push_back(pf);
        }
        const auto& list() const
        { return m_list; }

        InspectorWidgetBase* makeInspectorWidget(
                const QString& name,
                const QObject& model,
                QWidget* parent) const;

    private:
        std::vector<InspectorWidgetFactory*> m_list;
};

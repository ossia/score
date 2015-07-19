#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>

class AutomationControl;
class iscore_plugin_curve:
    public QObject,
    public iscore::FactoryInterface_QtInterface,
    public iscore::PluginControlInterface_QtInterface,
    public iscore::FactoryFamily_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
        Q_INTERFACES(
            iscore::FactoryInterface_QtInterface
            iscore::PluginControlInterface_QtInterface
            iscore::FactoryFamily_QtInterface
        )

    public:
        iscore_plugin_curve();
        virtual ~iscore_plugin_curve() = default;

        // Plugin control interface
        virtual iscore::PluginControlInterface* make_control(iscore::Presenter*) override;

        // Process & inspector
        virtual QVector<iscore::FactoryInterface*> factories(const QString& factoryName) override;

        // Curve segment factory family
        QVector<iscore::FactoryFamily> factoryFamilies() override;

    private:
        AutomationControl* m_control{};

};

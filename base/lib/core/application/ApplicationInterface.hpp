#pragma once
#include <iscore_lib_base_export.h>
#include <iscore/application/ApplicationContext.hpp>
namespace iscore
{
class Settings;
class Presenter;
class ApplicationRegistrar;
struct GUIApplicationContext;

/**
 * @brief The ApplicationInterface class
 *
 * This class provides an interface that should be implemented
 * and instantiated exactly once. In the main i-score application,
 * this is the \ref Application class.
 *
 * This class **SHALL** set the ApplicationInterface::m_instance member.
 *
 */
class ISCORE_LIB_BASE_EXPORT ApplicationInterface
{
public:
    ApplicationInterface();
    virtual ~ApplicationInterface();

    virtual const ApplicationContext& context() const = 0;

    virtual const ApplicationComponents& components() const = 0;

    static ApplicationInterface& instance();

    /**
     * @brief loadPluginData Utility method to load the minimal required data for plug-ins.
     *
     * For instance, the command system and the serialization system both require
     * all the datas in the plug-ins to work correctly (else, a command may crash
     * since a factory was not provided, or a file may not be able to be reloaded).
     *
     * This function takes care of loading the minimal set of elements from plugins so that
     * all the base functions of the software will work correctly.
     */
    void loadPluginData(
            const iscore::GUIApplicationContext& ctx,
            iscore::ApplicationRegistrar&,
            iscore::Settings& m_settings,
            iscore::Presenter& m_presenter);

    protected:
        static ApplicationInterface* m_instance;
};

}

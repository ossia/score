#pragma once
#include <iscore_lib_base_export.h>
#include <iscore/application/ApplicationContext.hpp>
namespace iscore
{
class Settings;
class Presenter;
class ApplicationRegistrar;
struct GUIApplicationContext;
class ISCORE_LIB_BASE_EXPORT ApplicationInterface
{
public:
    ApplicationInterface();
    virtual ~ApplicationInterface();
    virtual const ApplicationContext& context() const = 0;
    virtual const ApplicationComponents& components() const = 0;
    static ApplicationInterface& instance();

    void loadPluginData(
            const iscore::GUIApplicationContext& ctx,
            iscore::ApplicationRegistrar&,
            iscore::Settings& m_settings,
            iscore::Presenter& m_presenter);

    protected:
        static ApplicationInterface* m_instance;
};

}

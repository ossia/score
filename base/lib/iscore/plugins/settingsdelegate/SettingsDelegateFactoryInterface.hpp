#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore_lib_base_export.h>
namespace iscore
{
    class SettingsPresenter;
    class SettingsDelegatePresenterInterface;
    class SettingsDelegateModelInterface;
    class SettingsDelegateViewInterface;

    /**
     * @brief The SettingsDelegateFactory class
     *
     * Reimplement in order to provide custom settings for the plug-in.
     */
    class ISCORE_LIB_BASE_EXPORT SettingsDelegateFactory :
            public iscore::AbstractFactory<SettingsDelegateFactory>
    {
            ISCORE_ABSTRACT_FACTORY_DECL(
                    SettingsDelegateFactory,
                    "f18653bc-7ca9-44aa-a08b-4188d086b46e")

        public:
            virtual ~SettingsDelegateFactory();
            SettingsDelegatePresenterInterface* makePresenter(
                    iscore::SettingsDelegateModelInterface& m,
                    iscore::SettingsDelegateViewInterface& v,
                    QObject* parent);
            virtual SettingsDelegateViewInterface* makeView() = 0;
            virtual SettingsDelegateModelInterface* makeModel() = 0;

        protected:
            virtual SettingsDelegatePresenterInterface* makePresenter_impl(
                    iscore::SettingsDelegateModelInterface& m,
                    iscore::SettingsDelegateViewInterface& v,
                    QObject* parent) = 0;
    };

    class ISCORE_LIB_BASE_EXPORT SettingsDelegateFactoryList final :
            public ConcreteFactoryList<iscore::SettingsDelegateFactory>
    {
        public:
        using object_type = SettingsDelegateFactory;
    };

}

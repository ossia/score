#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore_lib_base_export.h>
namespace iscore
{
    class SettingsPresenter;
    class SettingsDelegatePresenter;
    class SettingsDelegateModel;
    class SettingsDelegateView;

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
            SettingsDelegatePresenter* makePresenter(
                    iscore::SettingsDelegateModel& m,
                    iscore::SettingsDelegateView& v,
                    QObject* parent);

            virtual SettingsDelegateView* makeView() = 0;

            virtual std::unique_ptr<SettingsDelegateModel> makeModel(
                        const iscore::ApplicationContext& ctx) = 0;

        protected:
            virtual SettingsDelegatePresenter* makePresenter_impl(
                    iscore::SettingsDelegateModel& m,
                    iscore::SettingsDelegateView& v,
                    QObject* parent) = 0;
    };

    class ISCORE_LIB_BASE_EXPORT SettingsDelegateFactoryList final :
            public ConcreteFactoryList<iscore::SettingsDelegateFactory>
    {
        public:
        using object_type = SettingsDelegateFactory;
    };


    template<typename Model_T, typename Presenter_T, typename View_T>
    class SettingsDelegateFactory_T : public SettingsDelegateFactory
    {
            std::unique_ptr<SettingsDelegateModel> makeModel(
                    const iscore::ApplicationContext& ctx) override
            {
                return std::make_unique<Model_T>(ctx);
            }

            iscore::SettingsDelegateView *makeView() override
            {
                return new View_T;
            }

            iscore::SettingsDelegatePresenter* makePresenter_impl(
                    iscore::SettingsDelegateModel& m,
                    iscore::SettingsDelegateView& v,
                    QObject* parent) override
            {
                return new Presenter_T{
                            safe_cast<Model_T&>(m),
                            safe_cast<View_T&>(v),
                            parent};
            }
    };

#define ISCORE_DECLARE_SETTINGS_FACTORY(Factory, Model, Presenter, View, Uuid) \
    class Factory final : \
        public iscore::SettingsDelegateFactory_T<Model, Presenter, View> \
    { \
            ISCORE_CONCRETE_FACTORY_DECL(Uuid) \
    };
}

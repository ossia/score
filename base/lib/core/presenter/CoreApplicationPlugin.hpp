#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <core/presenter/Presenter.hpp>
namespace iscore
{
class Presenter;
class CoreApplicationPlugin final :
        public QObject,
        public iscore::GUIApplicationContextPlugin
{
    public:
        CoreApplicationPlugin(const iscore::ApplicationContext& app,
                              Presenter& pres);

    private:
        Presenter& m_presenter;

        void newDocument();

        void load();
        void save();
        void saveAs();

        void quit();

        void openSettings();
        void about();
        GUIElements makeGUIElements() override;
};
}

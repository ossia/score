#pragma once
#include <iscore/plugins/panel/PanelFactoryInterface.hpp>
#include <iscore/plugins/panel/PanelModelInterface.hpp>
#include <iscore/plugins/panel/PanelPresenterInterface.hpp>
#include <iscore/plugins/panel/PanelViewInterface.hpp>

class DeviceExplorerWidget;
class DeviceExplorerModel;
class DeviceExplorerPanelPresenter : public iscore::PanelPresenterInterface
{
    public:
        DeviceExplorerPanelPresenter(iscore::Presenter* parent,
                                     iscore::PanelViewInterface* view);

        virtual void on_modelChanged() override;
        QString modelObjectName() const override
        {
            return "DeviceExplorerPanelModel";
        }

};

class DeviceExplorerPanelModel : public iscore::PanelModelInterface
{
        friend class DeviceExplorerPanelPresenter;
    public:

        DeviceExplorerPanelModel(iscore::DocumentModel* parent);
        DeviceExplorerPanelModel(const VisitorVariant& data, iscore::DocumentModel* parent);

        void serialize(const VisitorVariant&) const override;

        DeviceExplorerModel* deviceExplorer()
        {
            return m_model;
        }

    private:
        DeviceExplorerModel* m_model {};

};




class DeviceExplorerPanelView : public iscore::PanelViewInterface
{
        friend class DeviceExplorerPanelPresenter;
    public:
        DeviceExplorerPanelView(iscore::View* parent);
        virtual QWidget* getWidget() override;

        virtual Qt::DockWidgetArea defaultDock() const override
        {
            return Qt::LeftDockWidgetArea;
        }

    private:
        DeviceExplorerWidget* m_widget {};
};


class DeviceExplorerPanelFactory : public iscore::PanelFactoryInterface
{
        // PanelFactoryInterface interface
    public:
        QString name() const override { return "DeviceExplorerPanelModel"; }
        iscore::PanelViewInterface* makeView(iscore::View*) override;
        iscore::PanelPresenterInterface* makePresenter(
                iscore::Presenter* parent_presenter,
                iscore::PanelViewInterface* view) override;
        iscore::PanelModelInterface* makeModel(
                iscore::DocumentModel*) override;
        iscore::PanelModelInterface* makeModel(
                const VisitorVariant& data,
                iscore::DocumentModel* parent) override;
};

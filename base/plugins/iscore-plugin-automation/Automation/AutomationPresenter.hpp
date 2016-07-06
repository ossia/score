#pragma once
#include <Curve/Process/CurveProcessPresenter.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationLayerModel.hpp>
#include <Automation/AutomationView.hpp>

#include <Automation/Commands/ChangeAddress.hpp>
#include <Process/ProcessContext.hpp>
#include <State/MessageListSerialization.hpp>


namespace Automation
{
class LayerPresenter final :
        public Curve::CurveProcessPresenter<
            LayerModel,
            LayerView>
{
    public:
        LayerPresenter(
                const Curve::Style& style,
                const LayerModel& layer,
                LayerView* view,
                const Process::ProcessPresenterContext& context,
                QObject* parent):
            CurveProcessPresenter{style, layer, view, context, parent}
        {
            // TODO instead have a prettyNameChanged signal.
            con(m_layer.model(), &ProcessModel::addressChanged,
                this, &LayerPresenter::on_nameChanges);
            con(m_layer.model().metadata, &ModelMetadata::nameChanged,
                this, &LayerPresenter::on_nameChanges);

            connect(m_view, &LayerView::dropReceived,
                    this, [&] (const QMimeData* mime) {
                // TODO refactor with AddressEditWidget
                if(mime->formats().contains(iscore::mime::messagelist()))
                {
                    Mime<State::MessageList>::Deserializer des{*mime};
                    State::MessageList ml = des.deserialize();
                    if(ml.empty())
                        return;
                    auto& newAddr = ml[0].address;

                    if(newAddr == layer.model().address())
                        return;

                    if(newAddr.path.isEmpty())
                        return;

                    auto cmd = new ChangeAddress{layer.model(), newAddr};

                    CommandDispatcher<> disp{context.commandStack};
                    disp.submitCommand(cmd);
                }

            });
            m_view->setDisplayedName(m_layer.processModel().prettyName());
            m_view->showName(true);
        }

    private:
        void on_nameChanges()
        {
            m_view->setDisplayedName(m_layer.processModel().prettyName());
        }
};
}

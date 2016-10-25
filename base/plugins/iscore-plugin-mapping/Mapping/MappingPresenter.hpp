#pragma once
#include <Curve/Process/CurveProcessPresenter.hpp>
#include <Curve/CurveStyle.hpp>

#include <Mapping/MappingModel.hpp>
#include <Mapping/MappingLayerModel.hpp>
#include <Mapping/MappingView.hpp>

#include <Process/ProcessContext.hpp>

namespace Mapping
{
class LayerPresenter :
        public Curve::CurveProcessPresenter<
            Layer,
            LayerView>
{
    public:
        LayerPresenter(
                const Curve::Style& style,
                const Layer& layer,
                LayerView* view,
                const Process::ProcessPresenterContext& context,
                QObject* parent):
            CurveProcessPresenter{style, layer, view, context, parent}
        {
            con(m_layer.processModel(), &ProcessModel::sourceAddressChanged,
                this, &LayerPresenter::on_nameChanges);
            con(m_layer.processModel(), &ProcessModel::targetAddressChanged,
                this, &LayerPresenter::on_nameChanges);
            con(m_layer.processModel().metadata(), &iscore::ModelMetadata::NameChanged,
                this, &LayerPresenter::on_nameChanges);

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

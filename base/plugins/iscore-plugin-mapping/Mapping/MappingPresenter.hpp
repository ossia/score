#pragma once
#include <Curve/Process/CurveProcessPresenter.hpp>
#include <Curve/CurveStyle.hpp>

#include <Mapping/MappingModel.hpp>
#include <Mapping/MappingLayerModel.hpp>
#include <Mapping/MappingView.hpp>

#include <Process/ProcessContext.hpp>

namespace Mapping
{
class MappingPresenter :
        public Curve::CurveProcessPresenter<
            Layer,
            MappingView>
{
    public:
        MappingPresenter(
                const Curve::Style& style,
                const Layer& layer,
                MappingView* view,
                const Process::ProcessPresenterContext& context,
                QObject* parent):
            CurveProcessPresenter{style, layer, view, context, parent}
        {
            con(m_layer.processModel(), &ProcessModel::sourceAddressChanged,
                this, &MappingPresenter::on_nameChanges);
            con(m_layer.processModel(), &ProcessModel::targetAddressChanged,
                this, &MappingPresenter::on_nameChanges);
            con(m_layer.processModel().metadata, &ModelMetadata::nameChanged,
                this, &MappingPresenter::on_nameChanges);

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

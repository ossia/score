#pragma once
#include <Curve/Process/CurveProcessPresenter.hpp>
#include <Curve/CurveStyle.hpp>
#include <Interpolation/Layer.hpp>
#include <Interpolation/View.hpp>

namespace Interpolation
{
class Presenter :
        public Curve::CurveProcessPresenter<
            Layer,
            View>
{
    public:
        Presenter(
                const Curve::Style& style,
                const Layer& layer,
                View* view,
                const Process::ProcessPresenterContext& context,
                QObject* parent):
            CurveProcessPresenter{style, layer, view, context, parent}
        {
            con(m_layer.processModel(), &ProcessModel::addressChanged,
                this, &Presenter::on_nameChanges);
            con(m_layer.processModel().metadata, &ModelMetadata::NameChanged,
                this, &Presenter::on_nameChanges);

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

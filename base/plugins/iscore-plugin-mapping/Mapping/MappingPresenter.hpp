#pragma once
#include <Curve/Process/CurveProcessPresenter.hpp>
#include <Curve/CurveStyle.hpp>


#include "MappingModel.hpp"
#include "MappingLayerModel.hpp"
#include "MappingView.hpp"
class MappingPresenter :
        public CurveProcessPresenter<
            MappingLayerModel,
            MappingView>
{
    public:
        MappingPresenter(
                const MappingLayerModel& layer,
                MappingView* view,
                QObject* parent):
            CurveProcessPresenter{CurveStyle{}, layer, view, parent}
        {
          ISCORE_TODO;
          /*
            con(m_layer.model(), &MappingModel::addressChanged,
                this, [&] (const auto&)
            {
                m_view->setDisplayedName(m_layer.model().userFriendlyDescription());
            });

            m_view->setDisplayedName(m_layer.model().userFriendlyDescription());
            m_view->showName(true);
            */
        }
};

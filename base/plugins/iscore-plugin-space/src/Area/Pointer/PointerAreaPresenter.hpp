#pragma once
#include "src/Area/AreaPresenter.hpp"

namespace Space
{
class PointerAreaView;
class PointerAreaModel;
class PointerAreaPresenter : public AreaPresenter
{
    public:
        using model_type = PointerAreaModel;
        using view_type = PointerAreaView;
        PointerAreaPresenter(PointerAreaView *view,
                const PointerAreaModel &model,
                QObject* parent);

        void update() override;
        void on_areaChanged(ValMap) override;

};
}

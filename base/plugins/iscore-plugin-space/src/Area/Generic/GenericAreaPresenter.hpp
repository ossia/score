#pragma once
#include "src/Area/AreaPresenter.hpp"
#include "GenericAreaView.hpp"
#include <src/Area/Generic/AreaComputer.hpp>
namespace Space
{
class GenericAreaView;
class GenericAreaModel;

class GenericAreaPresenter : public AreaPresenter
{
        Q_OBJECT
    public:
        using model_type = GenericAreaModel;
        using view_type = GenericAreaView;

        GenericAreaPresenter(
                GenericAreaView *view,
                const GenericAreaModel &model,
                QObject* parent);

        void update() override;
        void on_areaChanged(ValMap) override;

    signals:
        void startCompute(QStringList formula, SpaceMap sm, ValMap vals);

    private:
        AreaComputer m_cp;

};
}

Q_DECLARE_METATYPE(spacelib::valued_area)
Q_DECLARE_METATYPE(Space::space2d)

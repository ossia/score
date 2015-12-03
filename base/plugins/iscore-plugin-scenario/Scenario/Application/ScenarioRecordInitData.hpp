#pragma once
#include <QPointF>
#include <QMetaType>

class LayerPresenter;
struct ScenarioRecordInitData
{
        ScenarioRecordInitData() {}
        ScenarioRecordInitData(const LayerPresenter* lp, QPointF p):
            presenter{lp},
            point{p}
        {
        }

        const LayerPresenter* presenter{};
        QPointF point;
};
Q_DECLARE_METATYPE(ScenarioRecordInitData)

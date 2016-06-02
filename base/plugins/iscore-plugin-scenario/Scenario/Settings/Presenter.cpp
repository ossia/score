#include "Presenter.hpp"
#include "Model.hpp"
#include "View.hpp"
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <QApplication>
#include <QStyle>
#include <iscore/command/SettingsCommand.hpp>

namespace Scenario
{
namespace Settings
{
Presenter::Presenter(
        Model& m,
        View& v,
        QObject *parent):
    iscore::SettingsDelegatePresenter{m, v, parent}
{
    con(v, &View::skinChanged,
        this, [&] (const auto& val) {
        if(val != m.getSkin())
        {
            m_disp.submitCommand<SetModelSkin>(this->model(this), val);
        }
    });

    con(m, &Model::SkinChanged, &v, &View::setSkin);
    v.setSkin(m.getSkin());

    con(v, &View::zoomChanged,
        this, [&] (const auto val) {
        if(val != m.getGraphicZoom())
        {
            m_disp.submitCommand<SetModelGraphicZoom>(this->model(this), 0.01*double(val));
        }
    });
    con(m, &Model::GraphicZoomChanged,
        this, [&] (const double z) {
        v.setZoom((100 * z));
    });
    v.setZoom(m.getGraphicZoom() * 100);


    con(v, &View::slotHeightChanged,
        this, [&] (const int& h) {
        if(h != m.getSlotHeight())
        {
            m_disp.submitCommand<SetModelSlotHeight>(this->model(this), qreal(h));
        }
    });
    con(m, &Model::SlotHeightChanged,
        this, [&] (const qreal h) {
        v.setSlotHeight(h);
    });
    v.setSlotHeight(qreal(m.getSlotHeight()));
}

QString Presenter::settingsName()
{
    return tr("Theme");
}

QIcon Presenter::settingsIcon()
{
    return QApplication::style()->standardIcon(QStyle::SP_DesktopIcon);
}


}
}

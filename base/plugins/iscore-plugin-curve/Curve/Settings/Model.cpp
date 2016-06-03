#include "Model.hpp"
#include <QSettings>

namespace Curve
{
namespace Settings
{

const QString Keys::simplificationRatio = QStringLiteral("iscore_plugin_curve/SimplificationRatio");
const QString Keys::simplify = QStringLiteral("iscore_plugin_curve/Simplify");
const QString Keys::mode = QStringLiteral("iscore_plugin_curve/Mode");
const QString Keys::playWhileRecording = QStringLiteral("iscore_plugin_curve/PlayWhileRecording");


Model::Model(const iscore::ApplicationContext& ctx)
{
    QSettings s;

    if(!s.contains(Keys::simplificationRatio))
    {
        setFirstTimeSettings();
    }

    m_simplificationRatio = s.value(Keys::simplificationRatio).toInt();
    m_simplify = s.value(Keys::simplify).toBool();
    m_mode = (Mode)s.value(Keys::mode).toInt();
    m_playWhileRecording = s.value(Keys::playWhileRecording).toBool();
}

int Model::getSimplificationRatio() const
{
    return m_simplificationRatio;
}

void Model::setSimplificationRatio(int simplificationRatio)
{
    if (m_simplificationRatio == simplificationRatio)
        return;

    m_simplificationRatio = simplificationRatio;

    QSettings s;
    s.setValue(Keys::simplificationRatio, m_simplificationRatio);
    emit SimplificationRatioChanged(simplificationRatio);
}

bool Model::getSimplify() const
{
    return m_simplify;
}

void Model::setSimplify(bool simplify)
{
    if (m_simplify == simplify)
        return;

    m_simplify = simplify;

    QSettings s;
    s.setValue(Keys::simplify, m_simplify);
    emit SimplifyChanged(simplify);
}

Mode Model::getMode() const
{
    return m_mode;
}

void Model::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;

    QSettings s;
    s.setValue(Keys::mode, static_cast<int>(m_mode));
    emit ModeChanged(mode);
}

bool Model::getPlayWhileRecording() const
{
    return m_playWhileRecording;
}

void Model::setPlayWhileRecording(bool playWhileRecording)
{
    if (m_playWhileRecording == playWhileRecording)
        return;

    m_playWhileRecording = playWhileRecording;

    QSettings s;
    s.setValue(Keys::playWhileRecording, m_playWhileRecording);
    emit PlayWhileRecordingChanged(playWhileRecording);
}


void Model::setFirstTimeSettings()
{
    m_simplificationRatio = 10;
    m_simplify = true;
    m_mode = Mode::Parameter;
    m_playWhileRecording = true;

    QSettings s;
    s.setValue(Keys::simplificationRatio, m_simplificationRatio);
    s.setValue(Keys::simplify, m_simplify);
    s.setValue(Keys::mode, static_cast<int>(m_mode));
    s.setValue(Keys::playWhileRecording, m_playWhileRecording);
}

}
}

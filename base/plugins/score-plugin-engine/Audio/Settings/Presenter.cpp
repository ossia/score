// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/Presenter.hpp>
#include <Audio/Settings/View.hpp>
#include <QApplication>
#include <QStyle>
#include <score/command/Command.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Audio::Settings::Model)
W_OBJECT_IMPL(Audio::Settings::View)
namespace Audio::Settings
{
Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::GlobalSettingsPresenter{m, v, parent}
{
  SETTINGS_PRESENTER(Driver);
  SETTINGS_PRESENTER(CardIn);
  SETTINGS_PRESENTER(CardOut);
  SETTINGS_PRESENTER(BufferSize);
  SETTINGS_PRESENTER(Rate);
  SETTINGS_PRESENTER(DefaultIn);
  SETTINGS_PRESENTER(DefaultOut);
}

QString Presenter::settingsName()
{
  return tr("Audio");
}

QIcon Presenter::settingsIcon()
{
  return QApplication::style()->standardIcon(QStyle::SP_MediaVolume);
}
}

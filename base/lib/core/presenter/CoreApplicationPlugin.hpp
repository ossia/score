#pragma once
#include <core/presenter/Presenter.hpp>
#include <iscore/plugins/application/GUIApplicationPlugin.hpp>
namespace iscore
{
class Presenter;
/**
 * @brief Base actions for the i-score software
 *
 * New document, load, open settings, etc.
 */
class ISCORE_LIB_BASE_EXPORT CoreApplicationPlugin final
    : public QObject,
      public iscore::GUIApplicationPlugin
{
public:
  CoreApplicationPlugin(
      const iscore::GUIApplicationContext& app, Presenter& pres);

private:
  Presenter& m_presenter;

  void newDocument();

  void load();
  void save();
  void saveAs();

  void close();
  void quit();

  void restoreLayout();

  void openSettings();
  void about();

  void loadStack();
  void saveStack();

  GUIElements makeGUIElements() override;
};
}

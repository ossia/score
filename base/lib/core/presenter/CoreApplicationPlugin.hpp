#pragma once
#include <core/presenter/Presenter.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
namespace score
{
class Presenter;
/**
 * @brief Base actions for the score software
 *
 * New document, load, open settings, etc.
 */
class SCORE_LIB_BASE_EXPORT CoreApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  CoreApplicationPlugin(
      const score::GUIApplicationContext& app, Presenter& pres);

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
  void help();
  void about();

  void loadStack();
  void saveStack();

  GUIElements makeGUIElements() override;
  void openProjectSettings();
};
}

#pragma once
#include <score_lib_base_export.h>

#include <functional>

class QWidget;
namespace score
{
struct GUIApplicationContext;

/**
 * @brief Implemented by an application plug-in able to join a collaborative
 * session hosted by another instance of score.
 *
 * The base application (start screen...) finds it by looking through the GUI
 * application plug-ins, so that it never depends on the network plug-in itself.
 */
class SCORE_LIB_BASE_EXPORT NetworkSessionInterface
{
public:
  virtual ~NetworkSessionInterface();

  /**
   * @brief Ask the user which session to join, then connect to it.
   *
   * `done(true)` is called once the session's document has been loaded,
   * `done(false)` if the user cancelled or the connection failed.
   */
  virtual void joinSession(QWidget* parent, std::function<void(bool)> done) = 0;
};

//! The first plug-in able to join sessions, or nullptr.
SCORE_LIB_BASE_EXPORT
NetworkSessionInterface* findNetworkSessionInterface(const GUIApplicationContext& ctx);
}

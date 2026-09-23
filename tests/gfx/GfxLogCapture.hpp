#pragma once

// Records the Qt messages logged while it is alive, for the tests that assert
// on the engine's warnings. The render and loader threads log too: one lock
// guards the chain of live captures and their records, so a handler call in
// flight on another thread never reaches a capture being destroyed. Every
// message is still forwarded to the previous handler so the test log stays
// complete. Captures nest (scoped, destroyed in reverse order): each live one
// records.

#include <QString>
#include <QtGlobal>

#include <initializer_list>
#include <mutex>
#include <string>
#include <vector>

namespace score::test::gfx
{

class LogCapture
{
public:
  struct Message
  {
    QtMsgType type;
    QString text;
  };

  LogCapture()
  {
    std::lock_guard lock{s_mutex};
    m_outer = s_current;
    s_current = this;
    if(!m_outer)
      s_previous = qInstallMessageHandler(&LogCapture::handler);
  }
  LogCapture(const LogCapture&) = delete;
  LogCapture& operator=(const LogCapture&) = delete;
  ~LogCapture()
  {
    std::lock_guard lock{s_mutex};
    s_current = m_outer;
    if(!m_outer)
      qInstallMessageHandler(s_previous);
  }

  std::vector<Message> messages() const
  {
    std::lock_guard lock{s_mutex};
    return m_messages;
  }

  void clear()
  {
    std::lock_guard lock{s_mutex};
    m_messages.clear();
  }

  /// Messages of `type` whose text contains every one of `needles`.
  int count(QtMsgType type, std::initializer_list<QStringView> needles = {}) const
  {
    std::lock_guard lock{s_mutex};
    int n = 0;
    for(const auto& m : m_messages)
      if(m.type == type && contains_all(m.text, needles))
        ++n;
    return n;
  }

  /// Messages of any type whose text contains every one of `needles`.
  int count(std::initializer_list<QStringView> needles) const
  {
    std::lock_guard lock{s_mutex};
    int n = 0;
    for(const auto& m : m_messages)
      if(contains_all(m.text, needles))
        ++n;
    return n;
  }

  /// Every recorded message, one per line, for an INFO().
  std::string dump() const
  {
    std::lock_guard lock{s_mutex};
    std::string s;
    for(const auto& m : m_messages)
      s += "  " + m.text.toStdString() + "\n";
    return s;
  }

private:
  static bool
  contains_all(const QString& text, std::initializer_list<QStringView> needles)
  {
    for(auto n : needles)
      if(!text.contains(n))
        return false;
    return true;
  }

  static void handler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
  {
    QtMessageHandler previous{};
    {
      std::lock_guard lock{s_mutex};
      for(auto* self = s_current; self; self = self->m_outer)
        self->m_messages.push_back({type, msg});
      previous = s_previous;
    }
    if(previous)
      previous(type, ctx, msg);
  }

  static inline std::mutex s_mutex;
  static inline LogCapture* s_current{};
  static inline QtMessageHandler s_previous{};

  LogCapture* m_outer{};
  std::vector<Message> m_messages;
};

}

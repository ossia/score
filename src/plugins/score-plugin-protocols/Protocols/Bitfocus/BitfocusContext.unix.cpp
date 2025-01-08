#include "BitfocusContext.hpp"

// #include <iostream>
namespace bitfocus
{

module_handler_base::module_handler_base(
    QString node_path, QString module_path, QString entrypoint)
{
  // Create socketpair
  socketpair(PF_LOCAL, SOCK_STREAM, 0, pfd);

  // Create env
  auto genv = QProcessEnvironment::systemEnvironment();
  genv.insert("CONNECTION_ID", "connectionId");
  genv.insert("VERIFICATION_TOKEN", "foobar");
  genv.insert("MODULE_MANIFEST", module_path + "/companion/manifest.json");
  genv.insert("NODE_CHANNEL_SERIALIZATION_MODE", "json");
  genv.insert("NODE_CHANNEL_FD", QString::number(pfd[1]).toUtf8());

  auto socket = new QSocketNotifier(pfd[0], QSocketNotifier::Read, this);
  QObject::connect(
      socket, &QSocketNotifier::activated, this, &module_handler_base::on_read);

  process.setProcessChannelMode(QProcess::ForwardedChannels);
  process.setProgram(node_path);
  process.setArguments({entrypoint});
  process.setWorkingDirectory(module_path);
  process.setProcessEnvironment(genv);

  process.start();

  // See https://forum.qt.io/topic/33964/solved-child-qprocess-that-dies-with-parent/10
}

module_handler_base::~module_handler_base()
{
  process.terminate();
}

void module_handler_base::on_read(QSocketDescriptor, QSocketNotifier::Type)
{
  ssize_t rl = ::read(pfd[0], buf, sizeof(buf));
  if(rl <= 0)
    return;
  queue.insert(queue.end(), buf, buf + rl);

  char* pos = queue.data();
  char* idx = queue.data();
  char* last_message_start = queue.data();
  char* const end = queue.data() + queue.size();

  do
  {
    idx = std::find(pos, end, '\n');
    if(idx < end)
    {
      last_message_start = idx;
      std::ptrdiff_t diff = idx - pos;
      std::string_view message(pos, diff);
      // std::cerr << "\n=========================\n <-- " << message << "\n";
      this->processMessage(message);
      pos = idx + 1;
      continue;
    }
  } while(idx < end);
  intptr_t processed_count = last_message_start - queue.data();
  queue.erase(queue.begin(), queue.begin() + processed_count);
}

void module_handler_base::do_write(std::string_view res)
{
  // std::cerr << "\n=========================\n --> " << res << "\n";
  ::write(pfd[0], res.data(), res.size());
}
}

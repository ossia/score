#include "BitfocusContext.hpp"

#include <signal.h>

#if defined(__linux__)
#include <sys/prctl.h>
#endif

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

  // Own session: the module and everything it spawns can then be signaled as a single
  // process group. PR_SET_PDEATHSIG makes the kernel terminate it if score goes away
  // without running any cleanup; the getppid check closes the fork/prctl race.
  process.setChildProcessModifier([parent = ::getpid()] {
    ::setsid();
#if defined(__linux__)
    ::prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
    if(::getppid() != parent)
      ::_exit(0);
  });

  process.start();
}

module_handler_base::~module_handler_base()
{
  if(const auto pid = process.processId(); pid > 0)
  {
    ::kill(-pid, SIGTERM);
    if(!process.waitForFinished(1000))
    {
      ::kill(-pid, SIGKILL);
      process.waitForFinished(1000);
    }
  }
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

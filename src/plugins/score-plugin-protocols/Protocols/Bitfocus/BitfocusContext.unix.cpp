#include "BitfocusContext.hpp"

#include <QDebug>
#include <QTimer>

#include <poll.h>
#include <signal.h>

#if defined(__linux__)
#include <sys/prctl.h>
#endif

namespace bitfocus
{

module_handler_base::module_handler_base(
    QString node_path, QString module_path, QString entrypoint, QString connection_id)
    : m_nodePath{std::move(node_path)}
    , m_modulePath{std::move(module_path)}
    , m_entrypoint{std::move(entrypoint)}
    , m_connectionId{std::move(connection_id)}
{
  start_process();
}

void module_handler_base::start_process()
{
  if(::socketpair(PF_LOCAL, SOCK_STREAM, 0, pfd) != 0)
  {
    qWarning() << "Bitfocus: cannot create the socket for" << m_modulePath;
    pfd[0] = pfd[1] = -1;
    return;
  }
  // Only the child's end is inherited
  ::fcntl(pfd[0], F_SETFD, FD_CLOEXEC);
#if defined(SO_NOSIGPIPE)
  {
    int one = 1;
    ::setsockopt(pfd[0], SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
  }
#endif

  // Create env
  auto genv = QProcessEnvironment::systemEnvironment();
  genv.insert("CONNECTION_ID", m_connectionId);
  genv.insert("VERIFICATION_TOKEN", "foobar");
  genv.insert("MODULE_MANIFEST", m_modulePath + "/companion/manifest.json");
  genv.insert("NODE_CHANNEL_SERIALIZATION_MODE", "json");
  genv.insert("NODE_CHANNEL_FD", QString::number(pfd[1]).toUtf8());

  socket = new QSocketNotifier(pfd[0], QSocketNotifier::Read, this);
  QObject::connect(
      socket, &QSocketNotifier::activated, this, &module_handler_base::on_read);

  process = std::make_unique<QProcess>();
  process->setProcessChannelMode(QProcess::ForwardedChannels);
  process->setProgram(m_nodePath);
  process->setArguments({m_entrypoint});
  process->setWorkingDirectory(m_modulePath);
  process->setProcessEnvironment(genv);

  // Own session: the module and everything it spawns can then be signaled as a single
  // process group. PR_SET_PDEATHSIG makes the kernel terminate it if score goes away
  // without running any cleanup; the getppid check closes the fork/prctl race.
  process->setChildProcessModifier([parent = ::getpid()] {
    ::setsid();
#if defined(__linux__)
    ::prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
    if(::getppid() != parent)
      ::_exit(0);
  });

  process->start();

  // Reads see EOF once the child exits
  ::close(pfd[1]);
  pfd[1] = -1;
}

void module_handler_base::release_process(int grace_ms)
{
  delete socket;
  socket = nullptr;
  queue.clear();
  scanned = 0;
  const int fd = std::exchange(pfd[0], -1);
  auto proc = process.release();
  if(!proc)
    return;

  // Given time to shut down, then terminated, without blocking
  auto done = std::make_shared<bool>(false);
  auto finish = [proc, fd, done] {
    if(std::exchange(*done, true))
      return;
    if(fd >= 0)
      ::close(fd);
    proc->deleteLater();
  };
  const auto pid = proc->processId();
  if(pid <= 0 || proc->state() == QProcess::NotRunning)
  {
    finish();
    return;
  }
  QObject::connect(proc, &QProcess::finished, proc, finish);
  QTimer::singleShot(grace_ms, proc, [pid] { ::kill(-pid, SIGTERM); });
  QTimer::singleShot(grace_ms + 1000, proc, [pid, finish] {
    ::kill(-pid, SIGKILL);
    finish();
  });
}

bool module_handler_base::restart_process()
{
  release_process(0);
  start_process();
  return true;
}

module_handler_base::~module_handler_base()
{
  release_process(release_grace_ms);
}

void module_handler_base::on_read(QSocketDescriptor, QSocketNotifier::Type)
{
  ssize_t rl = ::read(pfd[0], buf, sizeof(buf));
  if(rl == 0)
  {
    socket->setEnabled(false);
    // Queued: the handler may restart the process, which deletes this notifier
    QMetaObject::invokeMethod(this, [this] { on_process_exited(); }, Qt::QueuedConnection);
    return;
  }
  if(rl < 0)
    return;
  queue.insert(queue.end(), buf, buf + rl);
  process_queue();
}

void module_handler_base::process_queue()
{
  std::size_t start = 0;
  std::size_t from = scanned;
  for(;;)
  {
    auto nl = std::find(queue.begin() + from, queue.end(), '\n');
    if(nl == queue.end())
      break;
    const std::size_t len = (nl - queue.begin()) - start;
    this->processMessage(std::string_view(queue.data() + start, len));
    start += len + 1;
    from = start;
  }
  queue.erase(queue.begin(), queue.begin() + start);
  scanned = queue.size();
}

void module_handler_base::do_write(std::string_view res)
{
  while(!res.empty())
  {
#if defined(MSG_NOSIGNAL)
    auto n = ::send(pfd[0], res.data(), res.size(), MSG_NOSIGNAL);
#else
    auto n = ::write(pfd[0], res.data(), res.size());
#endif
    if(n <= 0)
    {
      if(n < 0 && errno == EINTR)
        continue;
      return;
    }
    res.remove_prefix(n);
  }
}
}

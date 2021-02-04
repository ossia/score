// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MessagesPanel.hpp"

#include <score/tools/Bind.hpp>

#include <core/application/SafeQApplication.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia/detail/logger.hpp>

#include <QDockWidget>
#include <QFileInfo>
#include <QListView>
#include <QMenu>

#include <wobjectimpl.h>

#include <deque>
W_OBJECT_IMPL(score::MessagesPanelDelegate)
namespace score
{
struct LogMessage
{
  QString message;
  QColor color;
};

class LogMessagesItemModel final : public QAbstractItemModel
{
public:
  LogMessagesItemModel(QObject* parent) : QAbstractItemModel{parent}, m_buffer{}
  {
    m_updateScheduler.setInterval(100);
    con(m_updateScheduler, &QTimer::timeout, this, &LogMessagesItemModel::update);
    m_updateScheduler.start();
  }

  std::size_t m_lastCount = 0;
  void update()
  {
    const auto n = m_buffer.size();
    if (m_lastCount < n)
    {
      beginInsertRows(QModelIndex(), m_lastCount, n);
      m_lastCount = n;
      endInsertRows();

      if (n > 600)
      {
        auto diff = n - 500;
        beginRemoveRows(QModelIndex(), 0, diff);
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + diff);
        m_lastCount = 500;
        endRemoveRows();
      }
    }
  }

  void push(LogMessage m) { m_buffer.push_back(std::move(m)); }

  void clear()
  {
    beginResetModel();
    m_buffer.clear();
    m_lastCount = 0;
    endResetModel();
  }

  QModelIndex index(int row, int column, const QModelIndex& parent) const override
  {
    return createIndex(row, column, nullptr);
  }
  QModelIndex parent(const QModelIndex& child) const override { return {}; }
  int rowCount(const QModelIndex& parent) const override { return m_lastCount; }
  int columnCount(const QModelIndex& parent) const override { return 1; }

  QVariant data(const QModelIndex& index, int role) const override
  {
    if (index.row() < (int32_t)m_buffer.size())
    {
      switch (role)
      {
        case Qt::DisplayRole:
          return m_buffer[index.row()].message;
        case Qt::BackgroundRole:
          return m_buffer[index.row()].color;
      }
    }

    return {};
  }

  std::deque<LogMessage> m_buffer;

  QTimer m_updateScheduler;
};

static MessagesPanelDelegate* g_messagesPanel{};

static void
LogToMessagePanel(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
  SafeQApplication::DebugOutput(type, context, msg);
  if (!g_messagesPanel)
    return;

  auto basename_arr = QFileInfo(context.file).baseName().toUtf8();
  auto filename = basename_arr.constData();

  QByteArray localMsg = msg.toLocal8Bit();
  switch (type)
  {
    case QtDebugMsg:
      g_messagesPanel->qtLog(
          fmt::format("Debug: {} ({}:{})", localMsg.constData(), filename, context.line));
      break;
    case QtInfoMsg:
      g_messagesPanel->qtLog(
          fmt::format("Info: {} ({}:{})", localMsg.constData(), filename, context.line));
      break;
    case QtWarningMsg:
      g_messagesPanel->qtLog(
          fmt::format("Warn: {} ({}:{})", localMsg.constData(), filename, context.line));
      break;
    case QtCriticalMsg:
      g_messagesPanel->qtLog(
          fmt::format("Critical: {} ({}:{})", localMsg.constData(), filename, context.line));
      break;
    case QtFatalMsg:
      g_messagesPanel->qtLog(
          fmt::format("Fatal: {} ({}:{})", localMsg.constData(), filename, context.line));
  }
}

MessagesPanelDelegate::MessagesPanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_itemModel{new LogMessagesItemModel{this}}
    , m_widget{new VisibilityNotifying<QListView>}
{
  g_messagesPanel = this;

  qInstallMessageHandler(LogToMessagePanel);
  m_widget->setModel(m_itemModel);
  m_widget->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
  connect(m_widget, &QListView::customContextMenuRequested, this, [=](const QPoint& pos) {
    QMenu m{};
    auto act = m.addAction(QObject::tr("Clear"));
    auto res = m.exec(QCursor::pos());
    if (res == act)
    {
      m_itemModel->clear();
    }
  });
}

VisibilityNotifying<QListView>* MessagesPanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus& MessagesPanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{
      false,
      false,
      Qt::LeftDockWidgetArea,
      10,
      QObject::tr("Message log"),
      "messages",
      QObject::tr("Ctrl+Shift+G")};

  return status;
}

void MessagesPanelDelegate::qtLog(const std::string& str)
{
  ossia::qt::run_async(this, [=] {
    if (m_itemModel && m_widget)
    {
      push(QString::fromStdString(str), score::log::dark4);
    }
  });
}

void MessagesPanelDelegate::push(const QString& str, const QColor& col)
{
  if(!m_widget->isVisible())
    return;

  m_itemModel->push({str, col});
  m_widget->scrollToBottom();
}

std::unique_ptr<score::PanelDelegate>
MessagesPanelDelegateFactory::make(const score::GUIApplicationContext& ctx)
{
  return std::make_unique<MessagesPanelDelegate>(ctx);
}
}

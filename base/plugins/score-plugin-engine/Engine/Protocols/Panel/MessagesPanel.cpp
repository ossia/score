// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MessagesPanel.hpp"
#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <QDockWidget>
#include <QListWidget>
#include <QMenu>
#include <ossia/detail/logger.hpp>
#include <Engine/OssiaLogger.hpp>
#include <deque>
//#include <boost/circular_buffer.hpp>
namespace Engine
{

struct LogMessage
{
  QString message;
  QColor color;
};

class LogMessagesItemModel final : public QAbstractItemModel
{
public:
  LogMessagesItemModel(QObject* parent)
      : QAbstractItemModel{parent}, m_buffer{}
  {
    m_updateScheduler.setInterval(100);
    con(m_updateScheduler, &QTimer::timeout, this,
        &LogMessagesItemModel::update);
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

  void push(LogMessage m)
  {
    m_buffer.push_back(std::move(m));
  }

  void clear()
  {
    beginResetModel();
    m_buffer.clear();
    m_lastCount = 0;
    endResetModel();
  }

  QModelIndex
  index(int row, int column, const QModelIndex& parent) const override
  {
    return createIndex(row, column, nullptr);
  }
  QModelIndex parent(const QModelIndex& child) const override
  {
    return {};
  }
  int rowCount(const QModelIndex& parent) const override
  {
    return m_lastCount;
  }
  int columnCount(const QModelIndex& parent) const override
  {
    return 1;
  }

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

MessagesPanelDelegate::MessagesPanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_itemModel{new LogMessagesItemModel{this}}
    , m_widget{new QListView}
{
  m_widget->setModel(m_itemModel);
  m_widget->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
  connect(
      m_widget, &QListView::customContextMenuRequested, this,
      [=](const QPoint& pos) {
        QMenu m{};
        auto act = m.addAction(QObject::tr("Clear"));
        auto res = m.exec(QCursor::pos());
        if (res == act)
        {
          m_itemModel->clear();
        }
      });
}

QWidget* MessagesPanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus& MessagesPanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{false, Qt::BottomDockWidgetArea, 0,
                                          QObject::tr("Messages"),
                                          QObject::tr("Ctrl+Shift+M")};

  return status;
}

void MessagesPanelDelegate::on_modelChanged(
    score::MaybeDocument oldm, score::MaybeDocument newm)
{
  disableConnections();
  QObject::disconnect(m_visible);

  if (!newm)
    return;

  if (auto qw = qobject_cast<QDockWidget*>(m_widget->parent()))
  {
    auto func = [=] (bool visible)
    {
      disableConnections();
      if (auto devices = getDeviceList(newm))
      {
        if (visible)
        {
          setupConnections(*devices);
        }
        devices->setLogging(visible);
      }
    };

    m_visible = QObject::connect(qw, &QDockWidget::visibilityChanged, this, func);

    func(qw->isVisible());
  }
}

void MessagesPanelDelegate::setupConnections(Device::DeviceList& devices)
{
  const auto dark1 = QColor(Qt::darkGray).darker();
  const auto dark2 = dark1.darker(); // almost darker than black
  const auto dark3 = QColor(Qt::darkRed).darker();
  m_inbound = QObject::connect(
      &devices, &Device::DeviceList::logInbound, m_widget,
      [=](const QString& str) {
        m_itemModel->push({str, dark1});
        m_widget->scrollToBottom();
      },
      Qt::QueuedConnection);

  m_outbound = QObject::connect(
      &devices, &Device::DeviceList::logOutbound, m_widget,
      [=](const QString& str) {
        m_itemModel->push({str, dark2});
        m_widget->scrollToBottom();
      },
      Qt::QueuedConnection);

  auto qt_sink = dynamic_cast<OssiaLogger*>(&*ossia::logger().sinks()[1]);
  if(qt_sink)
  {
    m_error = QObject::connect(qt_sink, &OssiaLogger::l,
                               this, [=] (Engine::level l, const QString& m) {
      m_itemModel->push({m, dark3});

      m_widget->scrollToBottom();
    }, Qt::QueuedConnection);
  }
}

void MessagesPanelDelegate::disableConnections()
{
  QObject::disconnect(m_inbound);
  QObject::disconnect(m_outbound);
  QObject::disconnect(m_error);
}

Device::DeviceList* MessagesPanelDelegate::getDeviceList(score::MaybeDocument newm)
{
  if (!newm)
    return nullptr;

  auto plug = newm->findPlugin<Explorer::DeviceDocumentPlugin>();
  if (!plug)
    return nullptr;
  return &plug->list();
}

std::unique_ptr<score::PanelDelegate>
MessagesPanelDelegateFactory::make(const score::GUIApplicationContext& ctx)
{
  return std::make_unique<MessagesPanelDelegate>(ctx);
}
}

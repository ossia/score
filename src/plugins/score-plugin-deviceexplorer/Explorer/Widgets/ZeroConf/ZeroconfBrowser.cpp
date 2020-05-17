// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/detail/config.hpp>
#if defined(OSSIA_DNSSD)
#include "ZeroconfBrowser.hpp"

#include <score/widgets/MarginLess.hpp>

#include <QAbstractItemView>
#include <QAction>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLineEdit>
#include <QList>
#include <QListView>
#include <QSpinBox>

#include <asio/io_service.hpp>
#include <asio/ip/basic_resolver.hpp>
#include <asio/ip/tcp.hpp>
#include <servus/qt/itemModel.h>
#include <servus/servus.h>
class QWidget;
W_REGISTER_ARGTYPE(QMap<QString, QByteArray>)
#include <wobjectimpl.h>
W_OBJECT_IMPL(ZeroconfBrowser)

ZeroconfBrowser::ZeroconfBrowser(const QString& service, QWidget* parent)
    : QObject{parent}, m_dialog{new QDialog{parent}}
{
  QGridLayout* lay = new QGridLayout;
  auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  connect(buttonBox, &QDialogButtonBox::accepted, this, &ZeroconfBrowser::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &ZeroconfBrowser::reject);

  lay->addWidget(buttonBox);
  m_dialog->setLayout(lay);

  try
  {
    m_serv = std::make_unique<servus::Servus>(service.toStdString());
    m_model = new servus::qt::ItemModel(*m_serv, this);
  }
  catch (const std::exception& e)
  {
    qDebug() << e.what();
  }
  catch (...)
  {
  }

  m_list = new QListView;
  m_list->setSelectionMode(QAbstractItemView::SingleSelection);
  m_list->setModel(m_model);

  connect(m_list, &QListView::doubleClicked, this, [=](const QModelIndex&) { accept(); });

  lay->addWidget(m_list);

  auto manualWidg = new QWidget;
  manualWidg->setLayout(new score::MarginLess<QHBoxLayout>);
  m_manualIp = new QLineEdit;
  m_manualPort = new QSpinBox;
  m_manualPort->setMinimum(1000);
  m_manualPort->setMaximum(65535);
  manualWidg->layout()->addWidget(m_manualIp);
  manualWidg->layout()->addWidget(m_manualPort);

  lay->addWidget(manualWidg);
}

ZeroconfBrowser::~ZeroconfBrowser()
{
  delete m_model;
}

QAction* ZeroconfBrowser::makeAction()
{
  QAction* act = new QAction(tr("Browse for server"), this);
  connect(act, &QAction::triggered, m_dialog, [=] { m_dialog->exec(); });

  return act;
}

void ZeroconfBrowser::accept()
{
  auto ip = m_manualIp->text();
  int port = m_manualPort->value();

  if (!ip.isEmpty() && port > 0)
  {
    sessionSelected("manual", ip, port, {});
    m_dialog->close();
    return;
  }
  ip.clear();

  auto selection = m_list->currentIndex();
  if (!selection.isValid())
  {
    m_dialog->close();
    return;
  }

  auto dat = m_model->itemData(selection);
  if (dat.isEmpty())
  {
    m_dialog->close();
    return;
  }

  auto name = dat.first().toString();

  QMap<QString, QByteArray> text;
  const int N = m_model->rowCount(selection);
  for (int i = 0; i < N; i++)
  {
    auto idx = m_model->index(i, 0, selection);
    if (!idx.isValid())
      continue;

    auto dat = m_model->itemData(idx);
    if (!dat.empty())
    {
      auto str = dat.first().toString();
      if (str.startsWith("servus_host = "))
      {
        str.remove("servus_host = ");
        if (ip.isEmpty())
          ip = std::move(str);
      }
      else if (str.startsWith("servus_port = "))
      {
        str.remove("servus_port = ");
        port = str.toInt();
      }
      else if (str.startsWith("servus_ip = "))
      {
        str.remove("servus_ip = ");
        ip = str;
      }
      else
      {
        auto res = str.split(" = ");
        if (res.size() == 2)
        {
          text.insert(res[0], res[1].toUtf8());
        }
      }
    }
  }

  try
  {
    asio::io_service io_service;
    asio::ip::tcp::resolver resolver(io_service);
    asio::ip::tcp::resolver::query query(ip.toStdString(), std::to_string(port));
    asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);
    if (iter->endpoint().address().is_loopback())
    {
      ip = "localhost";
    }
  }
  catch (...)
  {
  }

  if (!ip.isEmpty() && port > 0)
  {
    sessionSelected(name, ip, port, text);
    m_dialog->close();
  }
}

void ZeroconfBrowser::reject()
{
  m_dialog->close();
}
#endif

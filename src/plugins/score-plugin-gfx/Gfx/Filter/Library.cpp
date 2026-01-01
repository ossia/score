#include <Gfx/Filter/Library.hpp>
#include <Gfx/Filter/PreviewWidget.hpp>
#include <Gfx/Filter/Process.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <State/MessageListSerialization.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Process/Commands/EditPort.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/tools/File.hpp>
#include <ossia/gfx/texture_parameter.hpp>
#include <ossia/network/base/device.hpp>

#include <QDir>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryFile>
#include <QTextStream>
#include <QUrlQuery>

#include <wobjectimpl.h>

namespace Gfx::Filter
{

class ShadertoyDownloader : public QObject
{
  W_OBJECT(ShadertoyDownloader)
public:
  QByteArray response;
  ShadertoyDownloader(QObject* parent = nullptr) : QObject(parent)
  {
    m_networkManager = new QNetworkAccessManager(this);
  }

  void downloadShader(const QString& shaderId)
  {
    // Create POST request to Shadertoy API
    QNetworkRequest request(QUrl("https://www.shadertoy.com/shadertoy"));
    request.setRawHeader("Referer", "https://www.shadertoy.com/");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    
    // Prepare POST data
    QUrlQuery postData;
    postData.addQueryItem("s", "{\"shaders\":[\"" + shaderId + "\"]}");

    QNetworkReply* reply = m_networkManager->post(
        request, postData.toString(QUrl::FullyEncoded).toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply, shaderId]() {
      if(reply->error() == QNetworkReply::NoError)
      {
        response = reply->readAll();
      }
      else
      {
        qWarning() << "Failed to download shader:" << reply->errorString();
      }
      reply->deleteLater();
    });

    uint count = 0;
    bool waiting = reply->isFinished();
    while(!waiting)
    {
      qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
      count++;
      waiting = reply->isFinished();
      QThread::msleep(1);
      if(count > 3000)
      {
        waiting = true;
      }
    }
  }

  QNetworkAccessManager* m_networkManager{};
};

QSet<QString> DropHandler::mimeTypes() const noexcept
{
  return {"text/uri-list"}; // TODO
}

QSet<QString> LibraryHandler::acceptedFiles() const noexcept
{
  return {"fs"};
}

void LibraryHandler::setup(
    Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
{
  // TODO relaunch whenever library path changes...
  const auto& key = Metadata<ConcreteKey_k, Filter::Model>::get();
  QModelIndex node = model.find(key);
  if(node == QModelIndex{})
    return;

  categories.init(Metadata<PrettyName_k, Filter::Model>::get().toStdString(), node, ctx);
}

void LibraryHandler::addPath(std::string_view path)
{
  score::PathInfo file{path};

  Library::ProcessData pdata;
  pdata.prettyName
      = QString::fromUtf8(file.completeBaseName.data(), file.completeBaseName.size());
  pdata.key = Metadata<ConcreteKey_k, Filter::Model>::get();
  pdata.customData = QString::fromUtf8(path.data(), path.size());
  categories.add(file, std::move(pdata));
}

QWidget*
LibraryHandler::previewWidget(const QString& path, QWidget* parent) const noexcept
{
  if(!qEnvironmentVariableIsSet("SCORE_DISABLE_SHADER_PREVIEW"))
    return new ShaderPreviewWidget{path, parent};
  else
    return nullptr;
}

QWidget* LibraryHandler::previewWidget(
    const Process::Preset& path, QWidget* parent) const noexcept
{
  if(!qEnvironmentVariableIsSet("SCORE_DISABLE_SHADER_PREVIEW"))
    return new ShaderPreviewWidget{path, parent};
  else
    return nullptr;
}

QSet<QString> DropHandler::fileExtensions() const noexcept
{
  return {"frag", "glsl", "fs"};
}

void DropHandler::dropPath(
    std::vector<ProcessDrop>& vec, const score::FilePath& filename,
    const score::DocumentContext& ctx) const noexcept
{
  Process::ProcessDropHandler::ProcessDrop p;
  p.creation.key = Metadata<ConcreteKey_k, Gfx::Filter::Model>::get();
  p.creation.prettyName = filename.basename;
  p.creation.customData = filename.relative;

  vec.push_back(std::move(p));
}

void DropHandler::dropCustom(
    std::vector<ProcessDrop>& vec, const QMimeData& mime,
    const score::DocumentContext& ctx) const noexcept
{
  // FIXME handle multipass / multibuffer
  for(const auto& uri : mime.urls())
  {
    const auto& host = uri.host();
    if(uri.host().endsWith("shadertoy.com"))
    {
      // Extract shader ID from URL
      // URL format: https://www.shadertoy.com/view/XXXXXX
      QString path = uri.path();
      if(path.startsWith("/view/")) {
        QString shaderId = path.mid(6); // Remove "/view/"
        
        if(!shaderId.isEmpty()) {
          // Download shader asynchronously
          auto* downloader = new ShadertoyDownloader();
          downloader->downloadShader(shaderId);
          auto shader_json = downloader->response.toStdString();
          if(shader_json.empty())
          {
            continue;
          }
          isf::parser parser("", shader_json, 450, isf::parser::ShaderType::ShaderToy);
          auto isf = parser.write_isf();
          auto spec = parser.data();
          if(isf.empty())
          {
            continue;
          }
          // For immediate feedback, add a placeholder
          Process::ProcessDropHandler::ProcessDrop p;
          p.creation.key = Metadata<ConcreteKey_k, Gfx::Filter::Model>::get();
          p.creation.prettyName = "Shadertoy " + shaderId;
          p.setup = [isf](Process::ProcessModel& p, score::Dispatcher& d) {
            auto& filter = (Gfx::Filter::Model&)p;
            Gfx::ShaderSource source;
            source.vertex = "";
            source.fragment = QString::fromStdString(isf);
            auto cmd = new Gfx::ChangeShader{
                filter, source, score::IDocument::documentContext(p)};
            d.submit(cmd);
          };

          vec.push_back(std::move(p));
        }
      }
    }
  }
}

QSet<QString> VideoTextureDropHandler::mimeTypes() const noexcept
{
  return {score::mime::nodelist(), score::mime::messagelist()};
}

bool VideoTextureDropHandler::create(std::vector<ProcessDrop> &drops, const std::vector<State::Address> &addresses) const
{
  if(addresses.empty())
    return false;

  static const QString passthroughISF =
      QStringLiteral(R"_(/*{
  "CREDIT": "ossia score",
  "ISFVSN": "2",
  "DESCRIPTION": "Copy the input texture without changes",
  "CATEGORIES": [
    "Utility"
  ],
  "INPUTS": [ {
      "NAME": "inputImage",
      "TYPE": "image"
    }
  ]
}*/

void main() { gl_FragColor = IMG_THIS_PIXEL(inputImage); })_");

  for(auto& addr : addresses)
  {
    // For immediate feedback, add a placeholder
    Process::ProcessDropHandler::ProcessDrop p;
    p.creation.key = Metadata<ConcreteKey_k, Gfx::Filter::Model>::get();
    p.creation.prettyName = addr.device;
    p.setup = [addr](Process::ProcessModel& p, score::Dispatcher& d) {
      auto& filter = (Gfx::Filter::Model&)p;
      Gfx::ShaderSource source;
      source.fragment = passthroughISF;
      auto cmd = new Gfx::ChangeShader{filter, source, score::IDocument::documentContext(p)};
      d.submit(cmd);
      auto cmd2 = new Process::ChangePortAddress{*filter.inlets().front(), State::AddressAccessor{addr}};
      d.submit(cmd2);
    };
    drops.push_back(std::move(p));
  }
  return true;
}

bool VideoTextureDropHandler::isTexture(const State::Address &addr, const Device::DeviceList &devicelist) const noexcept
{
  if(auto* dev = devicelist.findDevice(addr.device))
    if(auto* ossia_dev = dev->getDevice())
      if(auto& node = ossia_dev->get_root_node(); auto p = node.get_parameter())
        if(auto texture_parameter = dynamic_cast<ossia::gfx::texture_parameter*>(p))
          return true;
  return false;
}

void VideoTextureDropHandler::dropCustom(std::vector<ProcessDrop> &drops, const QMimeData &mime, const score::DocumentContext &ctx) const noexcept
{
  const auto& devicelist = ctx.plugin<Explorer::DeviceDocumentPlugin>().list();
  if(mime.hasFormat(score::mime::nodelist()))
  {
    Mime<Device::FreeNodeList>::Deserializer des{mime};
    Device::FreeNodeList nl = des.deserialize();
    if(nl.empty())
      return;

    // Look for all the addresses that map to some texture input
    std::vector<State::Address> addresses;
    for(auto& np : nl)
    {
      if(isTexture(np.first, devicelist))
        addresses.push_back( np.first);
    }
    create(drops, addresses);
  }
  else if(mime.hasFormat(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();
    if(ml.empty())
      return;

    // Look for all the addresses that map to some texture input
    std::vector<State::Address> addresses;
    for(auto& mp : ml)
    {
      if(isTexture(mp.address.address, devicelist))
        addresses.push_back(mp.address.address);
    }
    create(drops, addresses);
  }
}

}

W_OBJECT_IMPL(Gfx::Filter::ShadertoyDownloader)

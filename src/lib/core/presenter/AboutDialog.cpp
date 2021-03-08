#include "AboutDialog.hpp"

#include <score/model/Skin.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QGuiApplication>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QScreen>

#include <score_git_info.hpp>
#if __has_include(<score_licenses.hpp>)
#include <score_licenses.hpp>
#endif

#include <QDate>
#include <QDesktopServices>
#include <QPainter>

namespace score
{
AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
    , m_windowSize(800, 700)
    , m_backgroundImage(score::get_image(":/about/about_background.png"))
    , m_catamaranFont("Catamaran", 13, QFont::Weight::Normal)
    , m_montserratFont("Montserrat", 12, QFont::Weight::Normal)
    , m_montserratLightFont("Montserrat", 12, QFont::Weight::Light)
    , m_mouseAreaOssiaScore(122, 30, 554, 130)
    , m_mouseAreaLabri(93, 370, 126, 40)
    , m_mouseAreaScrime(56, 435, 200, 70)
    , m_mouseAreaBlueYeti(76, 518, 160, 145)
{
  setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
  resize(m_windowSize.width(), m_windowSize.height());
  setMouseTracking(true);

  // map
  struct License
  {
    License() = default;
    License(const License&) = default;
    License(License&&) = default;
    License& operator=(const License&) = default;
    License& operator=(License&&) = default;
    License(QString u, QString l) : url{u}, license{l} { }
    License(QString u, const unsigned char* l) : url{u}
    {
      license = QString::fromUtf8(reinterpret_cast<const char*>(l));
    }
    License(QString u, QString lstart, const unsigned char* l) : url{u}, license{lstart}
    {
      license += QString::fromUtf8(reinterpret_cast<const char*>(l));
    }
    QString url;
    QString license;
  };

#if __has_include(<score_licenses.hpp>)
  std::map<QString, License> map;
  map["Score"] = License{"https://ossia.io", score_LICENSE};

  map["libossia"] = License{"https://ossia.io", ossia_LICENSE};

  map["Qt"] = License{"https://www.qt.io", "GNU General Public License v3"};

  map["Boost"] = License{"https://www.boost.org", "Boost Software License 1.0"};

  // In libossia
  map["Asio"] = License{"https://github.com/chriskohlhoff/asio", Asio_LICENSE};
  map["CMake"] = License{"https://www.cmake.org", "BSD 3-clause License"};
  map["TinySpline"] = License{"https://github.com/msteinbeck/tinyspline", "MIT License"};
  map["GSL"] = License{"https://github.com/Microsoft/GSL", GSL_LICENSE};

  map["libremidi"] = License{
      "https://github.com/jcelerier/libremidi",
      "Based on RtMidi (https://github.com/thestk/rtmidi) and "
      "ModernMIDI (https://github.com/ddiakopoulos/ModernMIDI)"};
  map["RtMidi"] = License{"https://github.com/thestk/rtmidi", libremidi_LICENSE};
  map["ModernMidi"] = License{"(https://github.com/ddiakopoulos/ModernMIDI", modernmidi_LICENSE};
  map["Servus"] = License{
      "https://github.com/jcelerier/Servus",
      "Based on https://github.com/HBPVIS/Servus\n",
      servus_LICENSE};

  map["SmallFunction"] = License{
      "https://github.com/jcelerier/SmallFunction",
      "Based on https://github.com/LoopPerfect/smallfunction\n",
      smallfun_LICENSE};

  map["bitset2"] = License{"https://github.com/ClaasBontus/bitset2", bitset2_LICENSE};
  map["Brigand"] = License{"https://github.com/edouarda/brigand", Brigand_LICENSE};
  map["Chobo"] = License{"https://github.com/Chobolabs/chobo-shl", chobo_LICENSE};
  map["ConcurrentQueue"]
      = License{"https://github.com/cameron314/concurrentqueue", concurrentqueue_LICENSE};
  map["ReaderWriterQueue"]
      = License{"https://github.com/cameron314/readerwriterqueue", readerwriterqueue_LICENSE};
  map["flat"] = License{
      "https://github.com/jcelerier/flat",
      "Based on https://github.com/pubby/flat\n",
      flat_LICENSE};
  map["flat_hash_map"] = License{
      "https://github.com/jcelerier/flat_hash_map",
      "Based on https://github.com/skarupke/flat_hash_map\n"};
  map["fmt"] = License{"https://github.com/fmtlib/fmt", fmt_LICENSE};
  map["frozen"] = License{"https://github.com/serge-sans-paille/frozen", frozen_LICENSE};
  map["hopscotch-map"] = License{"https://github.com/tessil/hopscotch-map", hopscotchmap_LICENSE};
  map["multi_index"] = License{"https://github.com/jcelerier/multi_index", multiindex_LICENSE};
  map["nano-signal-slot"]
      = License{"https://github.com/jcelerier/nano-signal-slot", nanosignal_LICENSE};
  map["OSCPack"] = License{"https://github.com/jcelerier/oscpack", "Boost License"};
  map["pybind11"] = License{"https://github.com/pybind/pybind11", pybind11_LICENSE};
  map["rapidjson"] = License{"https://github.com/miloyip/rapidjson", rapidjson_LICENSE};
  map["spdlog"] = License{"https://github.com/gabime/spdlog", spdlog_LICENSE};
  map["variant"] = License{"https://github.com/eggs-cpp/variant", variant_LICENSE};
  map["verdigris"] = License{"https://github.com/jcelerier/verdigris", verdigris_LICENSE};
  map["weakjack"] = License{
      "https://github.com/jcelerier/weakjack",
      "Based on https://github.com/x42/weakjack\nGNU "
      "General Public License version 2 (or later)"};
  map["websocketpp"] = License{"https://github.com/jcelerier/websocketpp", websocketpp_LICENSE};
  map["whereami"] = License{"https://github.com/gpakosz/whereami", whereami_LICENSE};

  // In score
  map["QMenuView"] = License{
      "https://github.com/pvanek/qlipper/blob/master/qmenuview",
      "CeCILL license. \nXINX\nCopyright Ulrich Van Den Hekke, "
      "(2007-2011)\nxinx@shadoware.org"};
  map["QProgressIndicator"] = License{
      "https://github.com/jcelerier/QProgressIndicator",
      qprogressindicator_LICENSE}; // TODO based on ...
  map["Qt-Color-Widgets"]
      = License{"https://github.com/jcelerier/Qt-Color-Widgets.git", qtcolorwidgets_LICENSE};
  map["PSIMPL"] = License{"http://psimpl.sf.net", "MPL 1.1"};

  // TODO ifdefs
  map["SDL"] = License{"", "Boost License"};
  map["Faust"] = License{"https://faust.grame.fr", "GNU General Public License"};
  map["libJACK"] = License{"https://jackaudio.org", "GNU Lesser General Public License"};
  map["Phidgets"] = License{"https://www.phidgets.com/", "Boost License"};
  map["FFMPEG"] = License{"https://ffmpeg.org", "GNU General Public License v3"};
  map["libusb"] = License{
      "https://libusb.info/", "GNU Lesser General Public License v2.1 or any later version"};

  //! Note : if there is a build error around here, you need to
  //! reset the _score_license_written CMake variable to zero
  //! (it will then add the missing license which have been added in a more
  //! recent commit - see score/src/lib/CMakeLists.txt at the end
  map["phantomstyle"] = License{"https://github.com/randrew/phantomstyle", phantomstyle_LICENSE};
  map["hsluvc"] = License{"", hsluv_LICENSE};
  map["VST"] = License{"https://steinberg.net", "VST is a trademark of Steinberg Media Technologies GmbH, registered in Europe and other countries."};

  // software list
  auto softwareList = new QListWidget{this};
  softwareList->move(307, 398);
  softwareList->resize(185, 263);

  softwareList->setFont(m_catamaranFont);
  softwareList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  softwareList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  for (const auto& item : map)
  {
    if (!item.second.license.isEmpty())
      softwareList->addItem(item.first);
  }
  for (int i = 0; i < softwareList->count(); i++)
  {
    softwareList->item(i)->setTextAlignment(Qt::AlignHCenter);
  }
  // license
  auto license = new QPlainTextEdit{this};
  license->move(537, 398);
  license->resize(222, 263);
  QFont smallCata = m_catamaranFont;
  smallCata.setPointSize(m_catamaranFont.pointSize() - 2);
  license->setFont(smallCata);
  license->setReadOnly(true);
  connect(softwareList, &QListWidget::currentTextChanged, this, [=](const QString& currentText) {
    auto& lic = map.at(currentText);
    license->setPlainText(lic.url + "\n\n" + lic.license);
  });
#endif
}

void AboutDialog::mousePressEvent(QMouseEvent* event)
{
  QPointF pos = event->localPos();
  if (m_mouseAreaOssiaScore.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("https://ossia.io/"));
  }
  else if (m_mouseAreaLabri.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("https://www.labri.fr/"));
  }
  else if (m_mouseAreaScrime.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("https://scrime.u-bordeaux.fr/"));
  }
  else if (m_mouseAreaBlueYeti.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("http://www.blueyeti.fr/"));
  }
  this->close();
}

void AboutDialog::mouseMoveEvent(QMouseEvent* event)
{
  QPointF pos = event->localPos();
  if (m_mouseAreaOssiaScore.contains(pos) || m_mouseAreaLabri.contains(pos)
      || m_mouseAreaScrime.contains(pos) || m_mouseAreaBlueYeti.contains(pos))
  {
    auto& skin = score::Skin::instance();
    this->setCursor(skin.CursorPointingHand);
  }
  else
  {
    this->setCursor(Qt::ArrowCursor);
  }
}
void AboutDialog::paintEvent(QPaintEvent* event)
{
  // draw background image
  QPainter painter(this);
  painter.drawImage(QPoint(0, 0), m_backgroundImage);

  // write version and commit
  {
    QString version_text = QStringLiteral("Version: %1.%2.%3")
                               .arg(SCORE_VERSION_MAJOR)
                               .arg(SCORE_VERSION_MINOR)
                               .arg(SCORE_VERSION_PATCH);
    if (std::string_view(SCORE_VERSION_EXTRA).size() != 0)
    {
      version_text += QStringLiteral("-%4").arg(SCORE_VERSION_EXTRA);
    }
    version_text += QStringLiteral(" “%5”\n").arg(SCORE_CODENAME);

    QString commit{GIT_COMMIT};

    if (!commit.isEmpty())
    {
      version_text += tr("Commit: %1\n").arg(commit);
    }
    painter.setPen(QColor{"#0092cf"});
    painter.setFont(m_montserratLightFont);
    painter.drawText(QRectF(0, 180, m_windowSize.width(), 50), Qt::AlignHCenter, version_text);
  }

  // write copyright
  {
    QString copyright_text
        = QString("Copyright © ossia 2014-" + QString::number(QDate::currentDate().year()))
          + "\nossia score is distributed under the GNU General Public License 3.0";

    painter.setPen(QColor{"#a0a0a0"});
    painter.setFont(m_montserratFont);
    painter.drawText(QRectF(0, 249, m_windowSize.width(), 50), Qt::AlignHCenter, copyright_text);
  }

  // write title above listview
  painter.setPen(QColor{"#aaaaaa"});
  QFont mb = m_montserratFont;
  mb.setBold(true);
  painter.setFont(mb);
  painter.drawText(QRectF(307, 368, 185, 28), Qt::AlignHCenter, tr("Project"));

  // write title above license
  painter.drawText(QRectF(537, 368, 222, 28), Qt::AlignHCenter, tr("License"));
}
}

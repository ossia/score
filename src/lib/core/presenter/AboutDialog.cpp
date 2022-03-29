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
    License(QString u, QString v)
      : url{u}
      , header{v}
    {
    }
    License(QString u, const unsigned char* l)
        : url{u}
    {
      auto s = reinterpret_cast<const char*>(l);
      license = QByteArray::fromRawData(s, strlen(s));
    }
    License(QString u, QString lstart, const unsigned char* l)
        : url{u}
        , header{lstart}
    {
      auto s = reinterpret_cast<const char*>(l);
      license = QByteArray::fromRawData(s, strlen(s));
    }
    QString url;
    QString header;
    QByteArray license;
  };

#if __has_include(<score_licenses.hpp>)
  struct CaseInsensitiveCompare {
    bool operator()(const QString& lhs, const QString& rhs) const noexcept {
      return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
    }
  };
  std::map<QString, License, CaseInsensitiveCompare> map;

  map["Qt"] = License{"https://www.qt.io", "GNU General Public License v3"};
  map["Boost"] = License{"https://www.boost.org", "Boost Software License 1.0"};
  map["CMake"] = License{"https://www.cmake.org", "BSD 3-clause License"};

  map["libossia"] = License{"https://ossia.io", ossia_LICENSE};
  map["score"] = License{"https://ossia.io", score_LICENSE};


  // In libossia
  map["TinySpline"]
      = License{"https://github.com/msteinbeck/tinyspline", "MIT License"};

  map["Brigand"]
      = License{"https://github.com/edouarda/brigand", Brigand_LICENSE};
  map["Catch2"]
      = License{"https://github.com/catchorg/Catch2", Catch2_LICENSE};
  map["ConcurrentQueue"] = License{
      "https://github.com/cameron314/concurrentqueue",
      concurrentqueue_LICENSE};
  map["dno"] = License{"https://github.com/thibaudk/dno", dno_LICENSE};
  map["DNS-SD"] = License{"", dnssd_LICENSE};
  map["dr_libs"] = License{"https://github.com/mackron/dr_libs", dr_libs_LICENSE};
  map["flat"] = License{
      "https://github.com/jcelerier/flat",
      "Based on https://github.com/pubby/flat\n",
      flat_LICENSE};
  map["flat_hash_map"] = License{
      "https://github.com/jcelerier/flat_hash_map",
      "Based on https://github.com/skarupke/flat_hash_map\n"};
  map["Flicks"] = License{"https://github.com/OculusVR/Flicks", Flicks_LICENSE};
  map["fmt"] = License{"https://github.com/fmtlib/fmt", fmt_LICENSE};
  map["Gist"] = License{"https://github.com/adamstark/Gist", Gist_LICENSE};
  map["GSL"] = License{"https://github.com/Microsoft/GSL", GSL_LICENSE};
  map["HAP Codec"] = License{"", hap_LICENSE};
  map["hsluvc"] = License{"", hsluv_LICENSE};
  map["hopscotch-map"] = License{
      "https://github.com/tessil/hopscotch-map", hopscotchmap_LICENSE};
  map["KFR"] = License{"https://github.com/kfrlib/kfr", kfr_LICENSE};
  map["libartnet"] = License{"https://github.com/OpenLightingArchitecture/libartnet", libartnet_LICENSE};
  map["libe131"] = License{"https://github.com/libpd/libpd", libe131_LICENSE};
  map["libremidi"] = License{
      "https://github.com/jcelerier/libremidi",
      "Based on RtMidi (https://github.com/thestk/rtmidi) and "
      "ModernMIDI (https://github.com/ddiakopoulos/ModernMIDI)"};
  map["RtMidi"]
      = License{"https://github.com/thestk/rtmidi", libremidi_LICENSE};
  map["libsamplerate"] = License{"https://github.com/mega-nerd/libsamplerate", libsamplerate_LICENSE};
  map["libsndfile"] = License{"https://github.com/mega-nerd/libsndfile", libsndfile_LICENSE};
  map["mdspan"] = License{"", mdspan_LICENSE};
  map["miniz"] = License{"", miniz_LICENSE};
  map["ModernMidi"] = License{
      "https://github.com/ddiakopoulos/ModernMIDI", modernmidi_LICENSE};
  map["nano-signal-slot"] = License{
      "https://github.com/jcelerier/nano-signal-slot", nanosignal_LICENSE};
  map["OSCPack"]
      = License{"https://github.com/jcelerier/oscpack", "Boost License"};
  map["Pure Data"] = License{"http://msp.ucsd.edu/software.html", pd_LICENSE};
  map["libpd"] = License{"https://github.com/libpd/libpd", libpd_LICENSE};
  //! Note : if there is a build error around here, you need to
  //! reset the _score_license_written CMake variable to zero
  //! (it will then add the missing license which have been added in a more
  //! recent commit - see score/src/lib/CMakeLists.txt at the end
  map["phantomstyle"] = License{
      "https://github.com/randrew/phantomstyle", phantomstyle_LICENSE};
  map["PortAudio"] = License{"https://github.com/PortAudio/PortAudio", portaudio_LICENSE};

  map["pybind11"]
      = License{"https://github.com/pybind/pybind11", pybind11_LICENSE};
  map["QCodeEditor"] = License{"", QCodeEditor_LICENSE};
  map["QProgressIndicator"] = License{
      "https://github.com/jcelerier/QProgressIndicator",
      qprogressindicator_LICENSE}; // TODO based on ...
  map["Qt-Color-Widgets"] = License{
      "https://github.com/jcelerier/Qt-Color-Widgets.git",
      qtcolorwidgets_LICENSE};

  map["rapidjson"]
      = License{"https://github.com/miloyip/rapidjson", rapidjson_LICENSE};
  map["ReaderWriterQueue"] = License{
      "https://github.com/cameron314/readerwriterqueue",
      readerwriterqueue_LICENSE};
  map["rnd"] = License{"https://github.com/jcelerier/rnd", rnd_LICENSE};
  map["Rubberband"] = License{"https://github.com/breakfastquay/rubberband", rubberband_LICENSE};
  map["Servus"] = License{
      "https://github.com/jcelerier/Servus",
      "Based on https://github.com/HBPVIS/Servus\n",
      servus_LICENSE};

  map["shmdata"] = License{"https://gitlab.com/sat-metalab/shmdata", shmdata_LICENSE};
#if defined(_WIN32)
  map["Spout"] = License{"https://spout.zeal.co/", spout_LICENSE};
#endif

#if defined(__APPLE__)
  map["Syphon"] = License{"http://syphon.v002.info/", syphon_LICENSE};
#endif

  map["SmallFunction"] = License{
      "https://github.com/jcelerier/SmallFunction",
      "Based on https://github.com/LoopPerfect/smallfunction\n",
      smallfun_LICENSE};

  map["snappy"] = License{"https://github.com/google/snappy", snappy_LICENSE};
  map["spdlog"] = License{"https://github.com/gabime/spdlog", spdlog_LICENSE};
  map["tuplet"] = License{"https://github.com/jcelerier/tuplet", tuplet_LICENSE};
  map["variant"]
      = License{"https://github.com/eggs-cpp/variant", variant_LICENSE};
  map["verdigris"]
      = License{"https://github.com/jcelerier/verdigris", verdigris_LICENSE};
  map["VST3 SDK"]
      = License{"https://steinberg.com", vst3_LICENSE};
  map["whereami"]
      = License{"https://github.com/gpakosz/whereami", whereami_LICENSE};
  map["weakjack"] = License{
      "https://github.com/jcelerier/weakjack",
      "Based on https://github.com/x42/weakjack\nGNU "
      "General Public License version 2 (or later)"};
  map["websocketpp"] = License{
      "https://github.com/jcelerier/websocketpp", websocketpp_LICENSE};
  map["wiiuse"]
      = License{"https://github.com/ossia/wiiuse", wiiuse_LICENSE};
  map["zipdownloader"]
      = License{"https://github.com/jcelerier/zipdownloader", zipdownloader_LICENSE};

  // In score
  map["QMenuView"] = License{
      "https://github.com/pvanek/qlipper/blob/master/qmenuview",
      "CeCILL license. \nXINX\nCopyright Ulrich Van Den Hekke, "
      "(2007-2011)\nxinx@shadoware.org"};
  map["PSIMPL"] = License{"http://psimpl.sf.net", "MPL 1.1"};

  // TODO ifdefs
  map["SDL"] = License{"https://libsdl.org", "Boost License"};
  map["Faust"]
      = License{"https://faust.grame.fr", "GNU General Public License"};
  map["libJACK"]
      = License{"https://jackaudio.org", "GNU Lesser General Public License"};
  map["Phidgets"] = License{"https://www.phidgets.com/", "Boost License"};
  map["FFMPEG"]
      = License{"https://ffmpeg.org", "GNU General Public License v3"};
  map["libusb"] = License{
      "https://libusb.info/",
      "GNU Lesser General Public License v2.1 or any later version"};

  map["VST"] = License{
      "https://steinberg.net",
      "VST is a trademark of Steinberg Media Technologies GmbH, registered in "
      "Europe and other countries."};
  map["ASIO"] = License{
      "https://steinberg.net",
      "ASIO is a trademark of Steinberg Media Technologies GmbH, registered in "
      "Europe and other countries."};
  map["NewTek NDI headers"] = License{
      "https://ndi.tv",
R"_(Copyright(c) 2014-2021, NewTek, inc.
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
)_"};

  // software list
  auto softwareList = new QListWidget{this};
  softwareList->move(307, 398);
  softwareList->resize(185, 263);

  softwareList->setFont(m_catamaranFont);
  softwareList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  softwareList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  for (const auto& item : map)
  {
    if (!item.second.license.isEmpty() || !item.second.header.isEmpty())
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
  connect(
      softwareList,
      &QListWidget::currentTextChanged,
      this,
      [=, m = std::move(map)](const QString& currentText) {
        auto& lic = m.at(currentText);
        license->setPlainText(lic.url + "\n\n" + lic.header + "\n" + lic.license);
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
    painter.drawText(
        QRectF(0, 180, m_windowSize.width(), 50),
        Qt::AlignHCenter,
        version_text);
  }

  // write copyright
  {
    QString copyright_text
        = QString("Copyright © ossia 2014-" + QString::number(QDate::currentDate().year()))
          + "\nossia score is distributed under the GNU General Public License 3.0";

    painter.setPen(QColor{"#a0a0a0"});
    painter.setFont(m_montserratFont);
    painter.drawText(
        QRectF(0, 249, m_windowSize.width(), 50),
        Qt::AlignHCenter,
        copyright_text);
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

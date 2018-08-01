#include "AboutDialog.hpp"
#include <QDate>
#include <QEvent>
#include <QDebug>
#include <QSize>
#include <QPicture>
#include <QFont>
#include <QPainter>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QDesktopServices>
#include <map>
#include <score_git_info.hpp>
#include <score/widgets/Pixmap.hpp>
#include <score_licenses.hpp>
namespace score {
AboutDialog::AboutDialog(QWidget *parent) :
  QDialog(parent),
  m_windowSize(492,437),
  m_backgroundImage(score::get_image(":/about/about_background.png")),
  m_catamaranFont("Catamaran", 13, QFont::Weight::Normal),
  m_montserratFont("Montserrat", 10, QFont::Weight::Normal),
  m_mouseAreaOssiaScore(102,13,295,84),
  m_mouseAreaLabri(16, 218, 116, 55),
  m_mouseAreaScrime(21, 275, 114, 40),
  m_mouseAreaBlueYeti(33, 321, 85, 84)
{
  setWindowFlag(Qt::FramelessWindowHint);
  resize(m_windowSize.width(),m_windowSize.height());
  setMouseTracking(true);

  if(auto scr = qApp->screens(); !scr.empty()) {
    auto dpi = scr.first()->devicePixelRatio();
    if(dpi >= 2.)
    {
      m_catamaranFont.setPointSize(13);
      m_montserratFont.setPointSize(10);
    }
    else
    {
      m_catamaranFont.setPointSize(11);
      m_montserratFont.setPointSize(9);
    }
  }

  // map
  struct License
  {
    License() = default;
    License(const License&) = default;
    License(License&&) = default;
    License& operator=(const License&) = default;
    License& operator=(License&&) = default;
    License(QString u, QString l): url{u}, license{l} { }
    License(QString u, const unsigned char* l): url{u}
    {
      license = QString::fromUtf8(reinterpret_cast<const char*>(l));
    }
    License(QString u, QString lstart, const unsigned char* l): url{u}, license{lstart}
    {
      license += QString::fromUtf8(reinterpret_cast<const char*>(l));
    }
    QString url;
    QString license;
  };

  std::map<QString, License> map;
  map["Score"] = License{"https://ossia.io", score_LICENSE};

  map["libossia"] = License{"https://ossia.io", ossia_LICENSE};

  map["Qt"] = License{"https://www.qt.io"
      , "GNU General Public License v3"};

  map["Boost"] = License{"https://www.boost.org"
      , "Boost Software License 1.0"};

  // In libossia
  map["Asio"] = License{"https://github.com/chriskohlhoff/asio", Asio_LICENSE};
  map["CMake"] = License{"https://www.cmake.org", "BSD 3-clause License"};
  map["TinySpline"] = License{"https://github.com/msteinbeck/tinyspline", "MIT License"};
  map["GSL"] = License{"https://github.com/Microsoft/GSL", GSL_LICENSE};

  map["RtMidi17"] = License{"https://github.com/jcelerier/RtMidi17",
                            "Based on RtMidi (https://github.com/thestk/rtmidi) and "
                            "ModernMIDI (https://github.com/ddiakopoulos/ModernMIDI)"};
  map["RtMidi"] = License{"https://github.com/thestk/rtmidi", rtmidi_LICENSE};
  map["ModernMidi"] = License{"(https://github.com/ddiakopoulos/ModernMIDI", modernmidi_LICENSE};
  map["Servus"] = License{"https://github.com/jcelerier/Servus", "Based on https://github.com/HBPVIS/Servus\n", servus_LICENSE};

  map["SmallFunction"] = License{"https://github.com/jcelerier/SmallFunction"
      , "Based on https://github.com/LoopPerfect/smallfunction\n", smallfun_LICENSE};

  map["bitset2"] = License{"https://github.com/ClaasBontus/bitset2", bitset2_LICENSE};
  map["Brigand"] = License{"https://github.com/edouarda/brigand", Brigand_LICENSE};
  map["Chobo"] = License{"https://github.com/Chobolabs/chobo-shl", chobo_LICENSE};
  map["ConcurrentQueue"] = License{"https://github.com/cameron314/concurrentqueue", concurrentqueue_LICENSE};
  map["ReaderWriterQueue"] = License{"https://github.com/cameron314/readerwriterqueue", readerwriterqueue_LICENSE};
  map["flat"] = License{"https://github.com/jcelerier/flat", "Based on https://github.com/pubby/flat\n", flat_LICENSE};
  map["flat_hash_map"] = License{"https://github.com/jcelerier/flat_hash_map"
      , "Based on https://github.com/skarupke/flat_hash_map\n" };
  map["fmt"] = License{"https://github.com/fmtlib/fmt", fmt_LICENSE};
  map["frozen"] = License{"https://github.com/serge-sans-paille/frozen", frozen_LICENSE};
  map["hopscotch-map"] = License{"https://github.com/tessil/hopscotch-map", hopscotchmap_LICENSE};
  map["multi_index"] = License{"https://github.com/jcelerier/multi_index", multiindex_LICENSE};
  map["nano-signal-slot"] = License{"https://github.com/jcelerier/nano-signal-slot", nanosignal_LICENSE};
  map["OSCPack"] = License{"https://github.com/jcelerier/oscpack", "Boost License"};
  map["Pure Data"] = License{"https://github.com/pure-data/pure-data.git", pd_LICENSE};
  map["pybind11"] = License{"https://github.com/pybind/pybind11", pybind11_LICENSE};
  map["rapidjson"] = License{"https://github.com/miloyip/rapidjson", rapidjson_LICENSE};
  map["spdlog"] = License{"https://github.com/gabime/spdlog", spdlog_LICENSE};
  map["variant"] = License{"https://github.com/eggs-cpp/variant", variant_LICENSE};
  map["verdigris"] = License{"https://github.com/jcelerier/verdigris", verdigris_LICENSE};
  map["weakjack"] = License{"https://github.com/jcelerier/weakjack"
                          , "Based on https://github.com/x42/weakjack\nGNU General Public License version 2 (or later)"};
  map["websocketpp"] = License{"https://github.com/jcelerier/websocketpp", websocketpp_LICENSE};
  map["whereami"] = License{"https://github.com/gpakosz/whereami", whereami_LICENSE};

  // In score
  map["QMenuView"] = License{"https://github.com/pvanek/qlipper/blob/master/qmenuview", "CeCILL license. \nXINX\nCopyright Ulrich Van Den Hekke, (2007-2011)\nxinx@shadoware.org"};
  map["QProgressIndicator"] = License{"https://github.com/jcelerier/QProgressIndicator", qprogressindicator_LICENSE}; // TODO based on ...
  map["Qt-Color-Widgets"] = License{"https://github.com/jcelerier/Qt-Color-Widgets.git", qtcolorwidgets_LICENSE};
  map["PSIMPL"] = License{"http://psimpl.sf.net", "MPL 1.1"};
  map["miniz"] = License{"https://github.com/richgel999/miniz", "MIT License"};
  map["desktopqqc2style"] = License{"https://anongit.kde.org/scratch/mart/desktopqqc2style.git", "MIT License"};

  // TODO ifdefs
  map["SDL"] = License{"", "Boost License"};
  map["Faust"] = License{"https://faust.grame.fr", "GNU General Public License"};
  map["PortAudio"] = License{"https://www.portaudio.com", portaudio_LICENSE};
  map["libJACK"] = License{"https://jackaudio.org", "GNU Lesser General Public License"};
  map["Phidgets"] = License{"https://www.phidgets.com/", "Boost License"};
  map["FFMPEG"] = License{"https://ffmpeg.org", "GNU General Public License v3"};
  map["libusb"] = License{"https://libusb.info/", "GNU Lesser General Public License v2.1 or any later version"};

  // software list
  auto softwareList = new QListWidget{this};
  softwareList->move(145,230);
  softwareList->resize(120,183);
  softwareList->setStyleSheet(R"_(
                              QListView::item:!selected:hover{
                              background-color:#415491;
                              color: #ffffff;
                              }
                              QListView::item:selected:active, QListView::item:selected:!active{
                              background-color:#73C1C6;
                              color: #1A2024;
                              }
                              QListView {
                              background-color: rgb(18,23,26);
                              margin:0px;
                              }

                              QScrollBar::right-arrow:horizontal, QScrollBar::left-arrow:horizontal
                              {
                              border: none;
                              background: none;
                              color: none;
                              }
                              QScrollBar::add-line:vertical {
                              border: none;
                              background: none;
                              }
                              QScrollBar::sub-line:vertical {
                              border: none;
                              background: none;
                              }
                              )_");
  softwareList->setFont(m_catamaranFont);
  softwareList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  softwareList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  for(const auto& item : map)
  {
    softwareList->addItem(item.first);
  }
  for(std::size_t i = 0; i < map.size(); i++)
  {
    softwareList->item(i)->setTextAlignment( Qt::AlignHCenter );
  }

  // license
  auto license = new QPlainTextEdit{this};
  license->move(280,230);
  license->resize(185,183);
  QFont smallCata = m_catamaranFont;
  smallCata.setPointSize(m_catamaranFont.pointSize() - 2);
  license->setFont(smallCata);
  license->setReadOnly(true);
  connect(softwareList,&QListWidget::currentTextChanged,this,
          [=] (const QString& currentText)
  {
    auto& lic = map.at(currentText);
    license->setPlainText(lic.url + "\n\n" + lic.license);
  });
}

void AboutDialog::mousePressEvent(QMouseEvent *event)
{
  QPointF pos = event->localPos();
  if(m_mouseAreaOssiaScore.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("https://ossia.io/"));
  }
  else if(m_mouseAreaLabri.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("https://www.labri.fr/"));
  }
  else if(m_mouseAreaScrime.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("https://scrime.u-bordeaux.fr/"));
  }
  else if(m_mouseAreaBlueYeti.contains(pos))
  {
    QDesktopServices::openUrl(QUrl("http://www.blueyeti.fr/"));
  }
  this->close();
}

void AboutDialog::mouseMoveEvent(QMouseEvent *event)
{
  QPointF pos = event->localPos();
  if( m_mouseAreaOssiaScore.contains(pos)
      || m_mouseAreaLabri.contains(pos)
      || m_mouseAreaScrime.contains(pos)
      || m_mouseAreaBlueYeti.contains(pos) )
  {
    this->setCursor(Qt::PointingHandCursor);
  }
  else
  {
    this->setCursor(Qt::ArrowCursor);
  }
}
void AboutDialog::paintEvent(QPaintEvent *event)
{
  QPen textPen(QColor("#0092cf"));
  QPen titleText(QColor("#aaaaaa"));
  QPen rectPen(QColor("#03c3dd"));
  QBrush rectBrush(QColor(18,23,26));

  // draw background image
  QPainter painter(this);
  painter.drawImage(QPoint(0,0), m_backgroundImage);

  // write version and commit
  {
    auto version_text = QStringLiteral("Version: %1.%2.%3-%4 “%5”\n")
        .arg(SCORE_VERSION_MAJOR)
        .arg(SCORE_VERSION_MINOR)
        .arg(SCORE_VERSION_PATCH)
        .arg(SCORE_VERSION_EXTRA)
        .arg(SCORE_CODENAME);

    QString commit{GIT_COMMIT};

    if (!commit.isEmpty())
    {
      version_text += tr("Commit: %1\n").arg(commit);
    }
    painter.setPen(textPen);
    painter.setFont(m_catamaranFont);
    painter.drawText(QRectF(0,100,m_windowSize.width(),60),
                     Qt::AlignHCenter,
                     version_text);
  }

  // write copyright
  {
    auto copyright_text = QString("Copyright © Ossia "+ QString::number(QDate::currentDate().year()));

    painter.setFont(m_montserratFont);
    painter.drawText(QRectF(0,160,m_windowSize.width(),30),
                     Qt::AlignHCenter,
                     copyright_text);
  }

  // write title above listview
  painter.setPen(titleText);
  QFont mb = m_montserratFont;
  mb.setBold(true);
  painter.setFont(mb);
  painter.drawText(QRectF(145,210,120,15),
                   Qt::AlignHCenter,
                   "Project");

  // write title above license
  painter.drawText(QRectF(280,210,185,15),
                   Qt::AlignHCenter,
                   "License");
}

}

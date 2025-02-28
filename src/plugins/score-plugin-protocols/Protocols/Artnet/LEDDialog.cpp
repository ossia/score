#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include <Protocols/Artnet/ArtnetProtocolSettingsWidget.hpp>
#include <Protocols/Artnet/LEDDialog.hpp>

namespace Protocols
{

AddLEDStripDialog::AddLEDStripDialog(
    int startUniverse, int startAddress, Mode mode, ArtnetProtocolSettingsWidget& parent)
    : QDialog{&parent}
    , m_mode{mode}
    , m_name{this}
    , m_buttons{
          QDialogButtonBox::StandardButton::Ok
              | QDialogButtonBox::StandardButton::Cancel,
          this}

{
  this->setLayout(&m_layout);
  m_layout.addRow(tr("Name"), &m_name);
  m_layout.addRow(tr("Address"), &m_address);
  m_layout.addRow(tr("Count"), &m_count);
  m_layout.addRow(tr("Spacing"), &m_spacing);
  m_layout.addRow(tr("Universe"), &m_universe);
  m_layout.addRow(tr("Channels per pixel"), &m_channels);
  m_layout.addRow(tr("Channels"), &m_channelComboLayout);

  switch(mode)
  {
    case Mode::Strip: {
      m_layout.addRow(tr("Pixel count"), &m_pixels);
      m_pixels.setVisible(true);
      m_width.setVisible(false);
      m_height.setVisible(false);
      m_depth.setVisible(false);
      break;
    }
    case Mode::Pane: {
      m_layout.addRow(tr("Width"), &m_width);
      m_layout.addRow(tr("Height"), &m_height);
      m_pixels.setVisible(false);
      m_width.setVisible(true);
      m_height.setVisible(true);
      m_depth.setVisible(false);
      break;
    }
    case Mode::Volume: {
      m_layout.addRow(tr("Width"), &m_width);
      m_layout.addRow(tr("Height"), &m_height);
      m_layout.addRow(tr("Depth"), &m_depth);
      m_pixels.setVisible(false);
      m_width.setVisible(true);
      m_height.setVisible(true);
      m_depth.setVisible(true);
      break;
    }
  }

  m_layout.addRow(tr("Reverse"), &m_reverse);

  m_layout.addItem(
      new QSpacerItem{0, 100, QSizePolicy::Expanding, QSizePolicy::Expanding});

  m_layout.addWidget(&m_buttons);
  m_layout.setAlignment(&m_buttons, Qt::AlignBottom);
  connect(&m_buttons, &QDialogButtonBox::accepted, this, &AddLEDStripDialog::accept);
  connect(&m_buttons, &QDialogButtonBox::rejected, this, &AddLEDStripDialog::reject);

  connect(
      &m_channels, &QSpinBox::valueChanged, this,
      &AddLEDStripDialog::on_channelsChanged);

  m_name.setText("strip.1");
  m_count.setRange(1, 512);
  m_spacing.setRange(0, 512);
  m_address.setRange(1, 65539 * 512);
  m_address.setValue(startAddress + 1);
  auto [umin, umax] = parent.universeRange();
  m_universe.setRange(umin, umax);
  m_universe.setValue(startUniverse);

  m_channels.setRange(1, 6);
  m_channels.setValue(3);
  m_channelCombos[0]->setCurrentIndex(0);
  m_channelCombos[1]->setCurrentIndex(1);
  m_channelCombos[2]->setCurrentIndex(2);
  m_pixels.setRange(1, 65539 * 512 / 3);
  m_pixels.setValue(16);
  m_width.setRange(1, 1024);
  m_width.setValue(8);
  m_height.setRange(1, 1024);
  m_height.setValue(8);
  m_depth.setRange(1, 1024);
  m_depth.setValue(8);
  m_reverse.setChecked(false);
}

void AddLEDStripDialog::on_channelsChanged(int count)
{
  std::vector<Artnet::Diode> current;
  for(auto& cur : m_channelCombos)
  {
    current.push_back((Artnet::Diode)cur->currentData().toInt());
    delete cur;
  }
  m_channelCombos.clear();

  for(int i = 0; i < count; i++)
  {
    auto cb = new QComboBox{this};

    cb->addItem("R", QVariant::fromValue(Artnet::Diode::R));
    cb->addItem("G", QVariant::fromValue(Artnet::Diode::G));
    cb->addItem("B", QVariant::fromValue(Artnet::Diode::B));
    cb->addItem("White", QVariant::fromValue(Artnet::Diode::White));
    cb->addItem("Warm", QVariant::fromValue(Artnet::Diode::WarmWhite));
    cb->addItem("Cold", QVariant::fromValue(Artnet::Diode::ColdWhite));
    cb->addItem("Amber", QVariant::fromValue(Artnet::Diode::Amber));
    cb->addItem("UV", QVariant::fromValue(Artnet::Diode::UV));
    cb->addItem("CCT", QVariant::fromValue(Artnet::Diode::CCT));
    cb->addItem("Empty", QVariant::fromValue(Artnet::Diode::Empty));

    if(i < std::ssize(current))
      cb->setCurrentIndex((int)current[i]);

    m_channelCombos.push_back(cb);
    m_channelComboLayout.addWidget(cb);
  }
}

std::vector<Artnet::Fixture> AddLEDStripDialog::fixtures() const noexcept
{
  std::vector<Artnet::Fixture> res;
  Artnet::Fixture f;
  f.fixtureName = this->m_name.text();
  f.modeName = "LED Strip";
  f.universe = m_universe.value();

  int channels_used = 0;
  switch(m_mode)
  {
    case Mode::Strip: {
      auto capa = Artnet::LEDStripLayout{};
      for(auto& cur : m_channelCombos)
        capa.diodes.push_back((Artnet::Diode)cur->currentData().toInt());

      capa.length = m_pixels.value();
      capa.reverse = m_reverse.isChecked();

      channels_used = capa.channels();

      f.led = std::move(capa);
      break;
    }
    case Mode::Pane: {
      auto capa = Artnet::LEDPaneLayout{};
      for(auto& cur : m_channelCombos)
        capa.diodes.push_back((Artnet::Diode)cur->currentData().toInt());

      capa.width = m_width.value();
      capa.height = m_height.value();

      channels_used = capa.channels();

      f.led = std::move(capa);
      break;
    }
    case Mode::Volume: {
      auto capa = Artnet::LEDVolumeLayout{};
      for(auto& cur : m_channelCombos)
        capa.diodes.push_back((Artnet::Diode)cur->currentData().toInt());

      capa.width = m_width.value();
      capa.height = m_height.value();
      capa.depth = m_depth.value();

      channels_used = capa.channels();

      f.led = std::move(capa);
      break;
    }
  }

  for(int i = 0; i < m_count.value(); i++)
  {
    f.address = m_address.value() - 1 + i * channels_used;
    res.push_back(f);
  }
  return res;
}
}
#endif

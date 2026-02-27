#include "WindowSettingsWidget.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Gfx/Window/CollapsibleSection.hpp>
#include <Gfx/Window/DesktopLayout.hpp>
#include <Gfx/Window/OutputMapping.hpp>
#include <Gfx/Window/OutputPreview.hpp>
#include <Gfx/WindowDevice.hpp>

#include <QSplitter>
W_OBJECT_IMPL(Gfx::FlatHeaderButton)
W_OBJECT_IMPL(Gfx::CollapsibleSection)
namespace Gfx
{

WindowSettingsWidget::WindowSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);

  m_modeCombo = new QComboBox;
  m_modeCombo->addItem(tr("Single Window"));
  m_modeCombo->addItem(tr("Background"));
  m_modeCombo->addItem(tr("Multi-Window Mapping"));
  layout->addRow(tr("Mode"), m_modeCombo);

  m_stack = new QStackedWidget;

  // Page 0: Single (empty)
  m_stack->addWidget(new QWidget);

  // Page 1: Background (empty)
  m_stack->addWidget(new QWidget);

  // Page 2: Multi-window (two tabs: Input Mapping + Desktop Layout)
  {
    auto* multiWidget = new QWidget;
    auto* multiLayout = new QVBoxLayout(multiWidget);

    // === Two-column layout: tabs on left, accordion inspector on right ===
    auto* splitter = new QSplitter(Qt::Horizontal);

    // --- Left side: Tab Widget with canvases ---
    m_tabWidget = new QTabWidget;

    // Tab 0: Input Mapping (canvas only)
    m_canvas = new OutputMappingCanvas;
    m_tabWidget->addTab(m_canvas, tr("Input Mapping"));

    // Tab 1: Desktop Layout
    m_desktopCanvas = new DesktopLayoutCanvas;
    m_tabWidget->addTab(m_desktopCanvas, tr("Desktop Layout"));

    splitter->addWidget(m_tabWidget);

    // --- Right side: Accordion inspector in a scroll area ---
    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setMinimumWidth(250);

    m_inspectorPanel = new QWidget;
    auto* inspectorLayout = new QVBoxLayout(m_inspectorPanel);
    inspectorLayout->setContentsMargins(0, 0, 0, 0);
    inspectorLayout->setSpacing(2);

    // -- Section: Global Settings (always visible) --
    {
      auto* section = new CollapsibleSection(tr("Global Settings"));
      auto* globalLayout = new QFormLayout;

      m_inputWidth = new QSpinBox;
      m_inputWidth->setRange(1, 16384);
      m_inputWidth->setValue(1920);
      m_inputHeight = new QSpinBox;
      m_inputHeight->setRange(1, 16384);
      m_inputHeight->setValue(1080);
      auto* resLayout = new QHBoxLayout;
      resLayout->addWidget(m_inputWidth);
      resLayout->addWidget(m_inputHeight);
      globalLayout->addRow(tr("Resolution"), resLayout);

      static int s_lastPreviewContent = 0;
      m_previewContentCombo = new QComboBox;
      m_previewContentCombo->addItem(tr("Black"));
      m_previewContentCombo->addItem(tr("Per-Output Test Card"));
      m_previewContentCombo->addItem(tr("Global Test Card"));
      m_previewContentCombo->addItem(tr("Output ID"));
      m_previewContentCombo->setCurrentIndex(s_lastPreviewContent);
      globalLayout->addRow(tr("Preview"), m_previewContentCombo);

      auto* snapCheck = new QCheckBox;
      snapCheck->setChecked(true);
      snapCheck->setToolTip(tr("Snap to borders in both canvases"));
      globalLayout->addRow(tr("Snap"), snapCheck);

      connect(snapCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_desktopCanvas->setSnapEnabled(checked);
        m_canvas->setSnapEnabled(checked);
      });

      auto* resetWarpBtn = new QPushButton(tr("Reset Warp"));
      globalLayout->addRow(resetWarpBtn);
      connect(resetWarpBtn, &QPushButton::clicked, this, [this] {
        if(m_canvas)
          m_canvas->resetWarp();
      });

      auto* syncPosBtn = new QPushButton(tr("Sync Positions"));
      syncPosBtn->setToolTip(
          tr("Force all preview windows to match their configured position, size and "
             "fullscreen state"));
      globalLayout->addRow(syncPosBtn);
      connect(syncPosBtn, &QPushButton::clicked, this, [this] { syncPreview(true); });

      connect(
          m_previewContentCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int idx) {
        s_lastPreviewContent = idx;
        if(m_preview)
          m_preview->setPreviewContent(static_cast<PreviewContent>(idx));
      });

      section->setContentLayout(globalLayout);
      inspectorLayout->addWidget(section);
    }

    // -- Section: Outputs (add/remove + selector buttons) --
    {
      auto* section = new CollapsibleSection(tr("Outputs"));
      auto* outputsLayout = new QVBoxLayout;

      auto* addRemoveLayout = new QHBoxLayout;
      auto* addBtn = new QPushButton(tr("Add"));
      auto* removeBtn = new QPushButton(tr("Remove"));
      addRemoveLayout->addWidget(addBtn);
      addRemoveLayout->addWidget(removeBtn);
      outputsLayout->addLayout(addRemoveLayout);

      // Row of index buttons for quick selection
      auto* buttonsWidget = new QWidget;
      m_outputButtonsLayout = new QHBoxLayout(buttonsWidget);
      m_outputButtonsLayout->setContentsMargins(0, 0, 0, 0);
      m_outputButtonsLayout->setSpacing(2);
      m_outputButtonsLayout->addStretch(1);
      outputsLayout->addWidget(buttonsWidget);

      connect(addBtn, &QPushButton::clicked, this, [this] {
        m_canvas->addOutput();

        // Set initial window properties from source rect x input resolution
        int inW = m_inputWidth->value();
        int inH = m_inputHeight->value();
        OutputMappingItem* newest = nullptr;
        for(auto* item : m_canvas->scene()->items())
          if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
            if(!newest || mi->outputIndex() > newest->outputIndex())
              newest = mi;
        if(newest)
        {
          auto r = newest->mapRectToScene(newest->rect());
          double srcX = r.x() / m_canvas->canvasWidth();
          double srcY = r.y() / m_canvas->canvasHeight();
          double srcW = r.width() / m_canvas->canvasWidth();
          double srcH = r.height() / m_canvas->canvasHeight();
          newest->windowPosition = QPoint(int(srcX * inW), int(srcY * inH));
          newest->windowSize
              = QSize(qMax(1, int(srcW * inW)), qMax(1, int(srcH * inH)));

          // Mirror onto desktop canvas
          if(m_desktopCanvas)
            m_desktopCanvas->addOutput(newest->windowPosition, newest->windowSize);
        }

        rebuildOutputButtons();
        syncPreview(true);
      });
      connect(removeBtn, &QPushButton::clicked, this, [this] {
        int removedIdx = m_selectedOutput;
        m_canvas->removeSelectedOutput();
        if(m_desktopCanvas && removedIdx >= 0)
          m_desktopCanvas->removeOutput(removedIdx);
        m_selectedOutput = -1;
        m_outputSectionsContainer->setVisible(false);
        rebuildOutputButtons();
        syncPreview(true);
      });

      section->setContentLayout(outputsLayout);
      inspectorLayout->addWidget(section);
    }

    // -- Per-output sections container (hidden when no selection) --
    m_outputSectionsContainer = new QWidget;
    auto* outputSectionsLayout = new QVBoxLayout(m_outputSectionsContainer);
    outputSectionsLayout->setContentsMargins(0, 0, 0, 0);
    outputSectionsLayout->setSpacing(2);
    m_outputSectionsContainer->setVisible(false);

    // -- Section: Source UV Mapping (expanded) --
    {
      auto* section = new CollapsibleSection(tr("Source UV Mapping"));
      auto* srcLayout = new QFormLayout;

      m_srcX = new QDoubleSpinBox;
      m_srcX->setRange(0.0, 1.0);
      m_srcX->setSingleStep(0.01);
      m_srcX->setDecimals(3);
      m_srcY = new QDoubleSpinBox;
      m_srcY->setRange(0.0, 1.0);
      m_srcY->setSingleStep(0.01);
      m_srcY->setDecimals(3);
      auto* srcPosLayout = new QHBoxLayout;
      srcPosLayout->addWidget(m_srcX);
      srcPosLayout->addWidget(m_srcY);
      m_srcPosPixelLabel = new QLabel;
      srcPosLayout->addWidget(m_srcPosPixelLabel);
      srcLayout->addRow(tr("Position"), srcPosLayout);

      m_srcW = new QDoubleSpinBox;
      m_srcW->setRange(0.01, 1.0);
      m_srcW->setSingleStep(0.01);
      m_srcW->setDecimals(3);
      m_srcW->setValue(1.0);
      m_srcH = new QDoubleSpinBox;
      m_srcH->setRange(0.01, 1.0);
      m_srcH->setSingleStep(0.01);
      m_srcH->setDecimals(3);
      m_srcH->setValue(1.0);
      auto* srcSizeLayout = new QHBoxLayout;
      srcSizeLayout->addWidget(m_srcW);
      srcSizeLayout->addWidget(m_srcH);
      m_srcSizePixelLabel = new QLabel;
      srcSizeLayout->addWidget(m_srcSizePixelLabel);
      srcLayout->addRow(tr("Size"), srcSizeLayout);

      section->setContentLayout(srcLayout);
      outputSectionsLayout->addWidget(section);
    }

    // -- Section: Window Properties (expanded) --
    {
      auto* section = new CollapsibleSection(tr("Window Properties"));
      auto* winLayout = new QFormLayout;

      m_screenCombo = new QComboBox;
      m_screenCombo->addItem(tr("Default"));
      for(auto* screen : qApp->screens())
        m_screenCombo->addItem(screen->name());
      winLayout->addRow(tr("Screen"), m_screenCombo);

      m_winPosX = new QSpinBox;
      m_winPosX->setRange(-32768, 32768);
      m_winPosY = new QSpinBox;
      m_winPosY->setRange(-32768, 32768);
      auto* posLayout = new QHBoxLayout;
      posLayout->addWidget(m_winPosX);
      posLayout->addWidget(m_winPosY);
      winLayout->addRow(tr("Window Pos"), posLayout);

      m_winWidth = new QSpinBox;
      m_winWidth->setRange(1, 16384);
      m_winWidth->setValue(1280);
      m_winHeight = new QSpinBox;
      m_winHeight->setRange(1, 16384);
      m_winHeight->setValue(720);
      auto* sizeLayout = new QHBoxLayout;
      sizeLayout->addWidget(m_winWidth);
      sizeLayout->addWidget(m_winHeight);
      winLayout->addRow(tr("Window Size"), sizeLayout);

      m_fullscreenCheck = new QCheckBox;
      winLayout->addRow(tr("Fullscreen"), m_fullscreenCheck);

      m_lockModeCombo = new QComboBox;
      m_lockModeCombo->addItem(tr("Free"));
      m_lockModeCombo->addItem(tr("Aspect Ratio"));
      m_lockModeCombo->addItem(tr("1:1 Pixel"));
      m_lockModeCombo->addItem(tr("Full Lock"));
      m_lockModeCombo->setToolTip(tr(
          "Free: no constraints\n"
          "Aspect Ratio: output window preserves input section aspect ratio\n"
          "1:1 Pixel: output window size matches input source rect pixels exactly\n"
          "Full Lock: cannot move or resize in graphics scenes"));
      winLayout->addRow(tr("Lock Mode"), m_lockModeCombo);

      section->setContentLayout(winLayout);
      outputSectionsLayout->addWidget(section);
    }

    // -- Section: Soft-Edge Blending (collapsed) --
    {
      auto* section = new CollapsibleSection(tr("Soft-Edge Blending"));
      auto* blendLayout = new QFormLayout;

      auto makeBlendRow = [&](const QString& label, QDoubleSpinBox*& widthSpin,
                              QDoubleSpinBox*& gammaSpin) {
        widthSpin = new QDoubleSpinBox;
        widthSpin->setRange(0.0, 0.5);
        widthSpin->setSingleStep(0.01);
        widthSpin->setDecimals(3);
        widthSpin->setValue(0.0);
        gammaSpin = new QDoubleSpinBox;
        gammaSpin->setRange(0.1, 4.0);
        gammaSpin->setSingleStep(0.1);
        gammaSpin->setDecimals(2);
        gammaSpin->setValue(2.2);
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel(tr("Width")));
        row->addWidget(widthSpin);
        row->addWidget(new QLabel(tr("Gamma")));
        row->addWidget(gammaSpin);
        row->addStretch(1);
        blendLayout->addRow(label, row);
      };

      makeBlendRow(tr("Left"), m_blendLeftW, m_blendLeftG);
      makeBlendRow(tr("Right"), m_blendRightW, m_blendRightG);
      makeBlendRow(tr("Top"), m_blendTopW, m_blendTopG);
      makeBlendRow(tr("Bottom"), m_blendBottomW, m_blendBottomG);

      section->setContentLayout(blendLayout);
      section->setExpanded(false);
      outputSectionsLayout->addWidget(section);
    }

    inspectorLayout->addWidget(m_outputSectionsContainer);
    inspectorLayout->addStretch(1);
    scrollArea->setWidget(m_inspectorPanel);
    splitter->addWidget(scrollArea);

    // Set splitter proportions: canvas gets more space
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    multiLayout->addWidget(splitter, 1);

    // === Wiring ===

    // Input mapping canvas selection -> update properties + cross-select desktop
    m_canvas->onSelectionChanged = [this](int idx) {
      if(m_syncing)
        return;
      m_syncing = true;
      m_selectedOutput = idx;
      m_outputSectionsContainer->setVisible(idx >= 0);
      rebuildOutputButtons();
      if(idx >= 0)
      {
        updatePropertiesFromSelection();
        if(m_desktopCanvas)
          m_desktopCanvas->selectItem(idx);
      }
      // Exit warp mode if selection changes to a different item
      if(m_canvas->inWarpMode() && m_canvas->inWarpMode())
        m_canvas->exitWarpMode();
      m_syncing = false;
    };

    m_canvas->onItemGeometryChanged = [this](int idx) {
      if(idx == m_selectedOutput)
        updatePropertiesFromSelection();
      // Sync desktop canvas item position/size from mapping item's window properties
      syncDesktopCanvasFromMappingItem(idx);
      syncPreview(false); // Input mapping changes don't sync preview positions
    };

    // Desktop canvas selection -> cross-select input mapping canvas
    m_desktopCanvas->onSelectionChanged = [this](int idx) {
      if(m_syncing)
        return;
      m_syncing = true;
      m_selectedOutput = idx;
      m_outputSectionsContainer->setVisible(idx >= 0);
      rebuildOutputButtons();
      if(idx >= 0)
      {
        // Exit warp mode when selection changes from desktop canvas
        if(m_canvas->inWarpMode())
          m_canvas->exitWarpMode();

        // Select same item in input mapping canvas
        if(m_canvas && m_canvas->scene())
        {
          m_canvas->scene()->clearSelection();
          for(auto* item : m_canvas->scene()->items())
          {
            if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
            {
              if(mi->outputIndex() == idx)
              {
                mi->setSelected(true);
                break;
              }
            }
          }
        }
        updatePropertiesFromSelection();
      }
      m_syncing = false;
    };

    // Desktop canvas item moved/resized -> update mapping item window properties + spinboxes
    m_desktopCanvas->onItemGeometryChanged = [this](int idx) {
      if(m_syncing)
        return;
      m_syncing = true;

      // Find the desktop item and read back its position/size
      for(auto* item : m_desktopCanvas->scene()->items())
      {
        if(auto* di = dynamic_cast<DesktopLayoutItem*>(item))
        {
          if(di->outputIndex() == idx)
          {
            auto sceneRect = di->mapRectToScene(di->rect());
            QPointF desktopPos = m_desktopCanvas->sceneToDesktop(sceneRect.topLeft());
            QSize desktopSize
                = m_desktopCanvas->sceneSizeToDesktop(sceneRect.size());

            // Update the mapping item
            if(m_canvas)
            {
              for(auto* mitem : m_canvas->scene()->items())
              {
                if(auto* mi = dynamic_cast<OutputMappingItem*>(mitem))
                {
                  if(mi->outputIndex() == idx)
                  {
                    mi->windowPosition
                        = QPoint((int)desktopPos.x(), (int)desktopPos.y());
                    mi->windowSize = desktopSize;

                    // Auto-detect screen
                    QRect winRect(mi->windowPosition, mi->windowSize);
                    int screenIdx = m_desktopCanvas->detectScreen(winRect);
                    mi->screenIndex = screenIdx;
                    break;
                  }
                }
              }
            }

            // Update spinboxes if this is the selected output
            if(idx == m_selectedOutput)
            {
              const QSignalBlocker b1(m_winPosX);
              const QSignalBlocker b2(m_winPosY);
              const QSignalBlocker b3(m_winWidth);
              const QSignalBlocker b4(m_winHeight);
              const QSignalBlocker b5(m_screenCombo);

              m_winPosX->setValue((int)desktopPos.x());
              m_winPosY->setValue((int)desktopPos.y());
              m_winWidth->setValue(desktopSize.width());
              m_winHeight->setValue(desktopSize.height());

              // Update screen combo from auto-detection
              QRect winRect(QPoint((int)desktopPos.x(), (int)desktopPos.y()), desktopSize);
              int screenIdx = m_desktopCanvas->detectScreen(winRect);
              m_screenCombo->setCurrentIndex(screenIdx + 1);
            }
            break;
          }
        }
      }

      syncPreview(true); // Desktop layout changes always sync preview positions
      m_syncing = false;
    };

    // Wire warp mode changes to preview sync
    m_canvas->onWarpChanged = [this] {
      syncPreview(false); // Warp is an input concern
    };

    // Wire window property spinbox changes -> mapping item + desktop canvas
    auto applyProps = [this] {
      applyPropertiesToSelection();
      syncDesktopCanvasFromMappingItem(m_selectedOutput);
      syncPreview(true); // Direct property edits are output concerns
    };
    connect(
        m_screenCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, applyProps);
    connect(m_winPosX, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_winPosY, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_winWidth, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_winHeight, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_fullscreenCheck, &QCheckBox::toggled, this, applyProps);
    connect(
        m_lockModeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, applyProps);
    connect(
        m_blendLeftW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applyProps);
    connect(
        m_blendLeftG, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applyProps);
    connect(
        m_blendRightW, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
        applyProps);
    connect(
        m_blendRightG, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
        applyProps);
    connect(
        m_blendTopW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applyProps);
    connect(
        m_blendTopG, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applyProps);
    connect(
        m_blendBottomW, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
        applyProps);
    connect(
        m_blendBottomG, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
        applyProps);

    // Wire source rect spinboxes to update canvas item geometry
    auto applySrc = [this] { applySourceRectToSelection(); };
    connect(m_srcX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applySrc);
    connect(m_srcY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applySrc);
    connect(m_srcW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applySrc);
    connect(m_srcH, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applySrc);

    // Update pixel labels when UV or input resolution changes
    auto updatePx = [this] { updatePixelLabels(); };
    connect(m_srcX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, updatePx);
    connect(m_srcY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, updatePx);
    connect(m_srcW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, updatePx);
    connect(m_srcH, qOverload<double>(&QDoubleSpinBox::valueChanged), this, updatePx);
    connect(m_inputWidth, qOverload<int>(&QSpinBox::valueChanged), this, updatePx);
    connect(m_inputHeight, qOverload<int>(&QSpinBox::valueChanged), this, updatePx);
    connect(
        m_screenCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, updatePx);

    // Update canvas aspect ratio and preview test card when input resolution changes
    auto updateAR = [this] {
      m_canvas->updateAspectRatio(m_inputWidth->value(), m_inputHeight->value());
      if(m_preview)
        m_preview->setInputResolution(
            QSize(m_inputWidth->value(), m_inputHeight->value()));
    };
    connect(m_inputWidth, qOverload<int>(&QSpinBox::valueChanged), this, updateAR);
    connect(m_inputHeight, qOverload<int>(&QSpinBox::valueChanged), this, updateAR);

    // Set initial aspect ratio
    m_canvas->updateAspectRatio(m_inputWidth->value(), m_inputHeight->value());

    m_stack->addWidget(multiWidget);
  }

  layout->addRow(m_stack);
  m_deviceNameEdit->setText("window");

  connect(
      m_modeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
      &WindowSettingsWidget::onModeChanged);

  setLayout(layout);
}

WindowSettingsWidget::~WindowSettingsWidget()
{
  if(this->m_modeCombo->currentIndex() == (int)WindowMode::MultiWindow)
  {
    if(auto doc = score::GUIAppContext().currentDocument())
    {
      auto& p = doc->plugin<Explorer::DeviceDocumentPlugin>();
      auto& dl = p.list();
      if(auto* self = dl.findDevice(this->m_deviceNameEdit->text()))
      {
        QMetaObject::invokeMethod(
            safe_cast<WindowDevice*>(self), &WindowDevice::reconnect);
      }
    }
  }
}

void WindowSettingsWidget::onModeChanged(int index)
{
  m_stack->setCurrentIndex(index);

  if(index != (int)WindowMode::MultiWindow)
  {
    // Destroy preview when leaving multi-window mode
    delete m_preview;
    m_preview = nullptr;
  }
  else
  {
    // Entering multi-window mode: always create preview
    if(!m_preview)
    {
      m_preview = new OutputPreviewWindows(this);
      m_preview->onFullscreenToggled = [this](int idx, bool fs) {
        // Update the mapping item
        if(m_canvas)
        {
          for(auto* item : m_canvas->scene()->items())
          {
            if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
            {
              if(mi->outputIndex() == idx)
              {
                mi->fullscreen = fs;
                break;
              }
            }
          }
        }
        // Update checkbox if this is the selected output
        if(idx == m_selectedOutput && m_fullscreenCheck)
        {
          const QSignalBlocker b(m_fullscreenCheck);
          m_fullscreenCheck->setChecked(fs);
        }
      };
      if(m_previewContentCombo)
        m_preview->setPreviewContent(
            static_cast<PreviewContent>(m_previewContentCombo->currentIndex()));
      m_preview->setInputResolution(
          QSize(m_inputWidth->value(), m_inputHeight->value()));
      syncPreview(true);
    }
  }
}

void WindowSettingsWidget::updatePropertiesFromSelection()
{
  if(!m_canvas || m_selectedOutput < 0)
    return;

  for(auto* item : m_canvas->scene()->items())
  {
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
    {
      if(mi->outputIndex() == m_selectedOutput)
      {
        // Block signals to avoid feedback loops when setting multiple spinboxes
        const QSignalBlocker b1(m_screenCombo);
        const QSignalBlocker b2(m_winPosX);
        const QSignalBlocker b3(m_winPosY);
        const QSignalBlocker b4(m_winWidth);
        const QSignalBlocker b5(m_winHeight);
        const QSignalBlocker b6(m_fullscreenCheck);
        const QSignalBlocker b6a(m_lockModeCombo);
        const QSignalBlocker b7(m_srcX);
        const QSignalBlocker b8(m_srcY);
        const QSignalBlocker b9(m_srcW);
        const QSignalBlocker b10(m_srcH);

        // Update source rect from canvas item geometry
        auto sceneRect = mi->mapRectToScene(mi->rect());
        m_srcX->setValue(sceneRect.x() / m_canvas->canvasWidth());
        m_srcY->setValue(sceneRect.y() / m_canvas->canvasHeight());
        m_srcW->setValue(sceneRect.width() / m_canvas->canvasWidth());
        m_srcH->setValue(sceneRect.height() / m_canvas->canvasHeight());

        // Update window properties
        m_screenCombo->setCurrentIndex(
            mi->screenIndex + 1); // +1 because index 0 is "Default"
        m_winPosX->setValue(mi->windowPosition.x());
        m_winPosY->setValue(mi->windowPosition.y());
        m_winWidth->setValue(mi->windowSize.width());
        m_winHeight->setValue(mi->windowSize.height());
        m_fullscreenCheck->setChecked(mi->fullscreen);
        m_lockModeCombo->setCurrentIndex(int(mi->lockMode));

        // Disable size spinboxes when size is locked
        const bool sizeLocked = (mi->lockMode == OutputLockMode::OneToOne);
        m_winWidth->setEnabled(!sizeLocked);
        m_winHeight->setEnabled(!sizeLocked);

        {
          const QSignalBlocker b11(m_blendLeftW);
          const QSignalBlocker b12(m_blendLeftG);
          const QSignalBlocker b13(m_blendRightW);
          const QSignalBlocker b14(m_blendRightG);
          const QSignalBlocker b15(m_blendTopW);
          const QSignalBlocker b16(m_blendTopG);
          const QSignalBlocker b17(m_blendBottomW);
          const QSignalBlocker b18(m_blendBottomG);

          m_blendLeftW->setValue(mi->blendLeft.width);
          m_blendLeftG->setValue(mi->blendLeft.gamma);
          m_blendRightW->setValue(mi->blendRight.width);
          m_blendRightG->setValue(mi->blendRight.gamma);
          m_blendTopW->setValue(mi->blendTop.width);
          m_blendTopG->setValue(mi->blendTop.gamma);
          m_blendBottomW->setValue(mi->blendBottom.width);
          m_blendBottomG->setValue(mi->blendBottom.gamma);
        }

        updatePixelLabels();
        return;
      }
    }
  }
}

void WindowSettingsWidget::applyPropertiesToSelection()
{
  if(!m_canvas || m_selectedOutput < 0)
    return;

  for(auto* item : m_canvas->scene()->items())
  {
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
    {
      if(mi->outputIndex() == m_selectedOutput)
      {
        mi->screenIndex
            = m_screenCombo->currentIndex() - 1; // -1 because index 0 is "Default"
        mi->windowPosition = QPoint(m_winPosX->value(), m_winPosY->value());
        mi->fullscreen = m_fullscreenCheck->isChecked();

        auto oldLockMode = mi->lockMode;
        mi->lockMode = OutputLockMode(m_lockModeCombo->currentIndex());

        // 1:1 pixel lock: compute window size from source rect x input resolution
        if(mi->lockMode == OutputLockMode::OneToOne)
        {
          auto sceneRect = mi->mapRectToScene(mi->rect());
          double srcW = sceneRect.width() / m_canvas->canvasWidth();
          double srcH = sceneRect.height() / m_canvas->canvasHeight();
          int pxW = qMax(1, (int)(srcW * m_inputWidth->value()));
          int pxH = qMax(1, (int)(srcH * m_inputHeight->value()));
          mi->windowSize = QSize(pxW, pxH);

          const QSignalBlocker bw(m_winWidth);
          const QSignalBlocker bh(m_winHeight);
          m_winWidth->setValue(pxW);
          m_winHeight->setValue(pxH);
          m_winWidth->setEnabled(false);
          m_winHeight->setEnabled(false);
        }
        else if(mi->lockMode == OutputLockMode::AspectRatio)
        {
          // Enforce aspect ratio: adjust height to match source aspect
          auto sceneRect = mi->mapRectToScene(mi->rect());
          double srcW = sceneRect.width() / m_canvas->canvasWidth();
          double srcH = sceneRect.height() / m_canvas->canvasHeight();
          double aspect = (srcW * m_inputWidth->value())
                          / qMax(1.0, srcH * m_inputHeight->value());

          int w = m_winWidth->value();
          int h = qMax(1, (int)(w / aspect));
          mi->windowSize = QSize(w, h);

          const QSignalBlocker bh(m_winHeight);
          m_winHeight->setValue(h);
          m_winWidth->setEnabled(true);
          m_winHeight->setEnabled(false);
        }
        else
        {
          mi->windowSize = QSize(m_winWidth->value(), m_winHeight->value());
          m_winWidth->setEnabled(true);
          m_winHeight->setEnabled(true);
        }

        // Update locked state if changed
        if(mi->lockMode != oldLockMode)
        {
          mi->applyLockedState();
          // Also update the desktop canvas item
          if(m_desktopCanvas)
          {
            for(auto* ditem : m_desktopCanvas->scene()->items())
            {
              if(auto* di = dynamic_cast<DesktopLayoutItem*>(ditem))
              {
                if(di->outputIndex() == mi->outputIndex())
                {
                  di->lockMode = mi->lockMode;
                  di->applyLockedState();
                  break;
                }
              }
            }
          }
        }

        mi->blendLeft = {(float)m_blendLeftW->value(), (float)m_blendLeftG->value()};
        mi->blendRight = {(float)m_blendRightW->value(), (float)m_blendRightG->value()};
        mi->blendTop = {(float)m_blendTopW->value(), (float)m_blendTopG->value()};
        mi->blendBottom
            = {(float)m_blendBottomW->value(), (float)m_blendBottomG->value()};
        mi->update(); // Repaint to show blend zones
        // NOTE: caller is responsible for calling syncPreview with the correct flag
        return;
      }
    }
  }
}

void WindowSettingsWidget::applySourceRectToSelection()
{
  if(!m_canvas || m_selectedOutput < 0)
    return;

  for(auto* item : m_canvas->scene()->items())
  {
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
    {
      if(mi->outputIndex() == m_selectedOutput)
      {
        QRectF newSceneRect(
            m_srcX->value() * m_canvas->canvasWidth(),
            m_srcY->value() * m_canvas->canvasHeight(),
            m_srcW->value() * m_canvas->canvasWidth(),
            m_srcH->value() * m_canvas->canvasHeight());

        // Temporarily disable onChanged to avoid feedback loop
        auto savedCallback = std::move(mi->onChanged);
        mi->setPos(0, 0);
        mi->setRect(newSceneRect);
        mi->onChanged = std::move(savedCallback);
        syncPreview(false); // Source rect is an input concern
        return;
      }
    }
  }
}

void WindowSettingsWidget::updatePixelLabels()
{
  if(!m_inputWidth || !m_inputHeight)
    return;

  int w = m_inputWidth->value();
  int h = m_inputHeight->value();

  int pxX = int(m_srcX->value() * w);
  int pxY = int(m_srcY->value() * h);
  int pxW = qMax(1, int(m_srcW->value() * w));
  int pxH = qMax(1, int(m_srcH->value() * h));

  if(m_srcPosPixelLabel)
    m_srcPosPixelLabel->setText(QStringLiteral("(%1, %2 px)").arg(pxX).arg(pxY));

  if(m_srcSizePixelLabel)
    m_srcSizePixelLabel->setText(QStringLiteral("%1 x %2 px").arg(pxW).arg(pxH));

  // If 1:1 pixel lock is active, always update window size from source rect
  const auto curLockMode = m_lockModeCombo
                               ? OutputLockMode(m_lockModeCombo->currentIndex())
                               : OutputLockMode::Free;
  if(curLockMode == OutputLockMode::OneToOne && m_selectedOutput >= 0)
  {
    const QSignalBlocker bw(m_winWidth);
    const QSignalBlocker bh(m_winHeight);
    m_winWidth->setValue(pxW);
    m_winHeight->setValue(pxH);
    applyPropertiesToSelection();
    syncDesktopCanvasFromMappingItem(m_selectedOutput);
    syncPreview(true); // Window size changed as a consequence — sync preview
  }
  else if(curLockMode == OutputLockMode::AspectRatio && m_selectedOutput >= 0)
  {
    // Recompute height from current width to maintain aspect ratio
    double aspect = (double)pxW / qMax(1, pxH);
    int curW = m_winWidth->value();
    int newH = qMax(1, (int)(curW / aspect));

    const QSignalBlocker bh(m_winHeight);
    m_winHeight->setValue(newH);
    applyPropertiesToSelection();
    syncDesktopCanvasFromMappingItem(m_selectedOutput);
    syncPreview(true); // Window size changed as a consequence — sync preview
  }
  // Auto-match window pos/size when screen is "Default" (combo index 0 = screenIndex -1)
  else if(m_screenCombo && m_screenCombo->currentIndex() == 0 && m_selectedOutput >= 0)
  {
    const QSignalBlocker bx(m_winPosX);
    const QSignalBlocker by(m_winPosY);
    const QSignalBlocker bw(m_winWidth);
    const QSignalBlocker bh(m_winHeight);

    m_winPosX->setValue(pxX);
    m_winPosY->setValue(pxY);
    m_winWidth->setValue(pxW);
    m_winHeight->setValue(pxH);

    applyPropertiesToSelection();
    syncDesktopCanvasFromMappingItem(m_selectedOutput);
  }
}

void WindowSettingsWidget::rebuildOutputButtons()
{
  if(!m_outputButtonsLayout || !m_canvas)
    return;

  // Clear existing buttons
  while(auto* item = m_outputButtonsLayout->takeAt(0))
  {
    delete item->widget();
    delete item;
  }

  // Collect output indices
  std::vector<int> indices;
  for(auto* item : m_canvas->scene()->items())
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
      indices.push_back(mi->outputIndex());
  std::sort(indices.begin(), indices.end());

  // Color palette matching OutputPreview/DesktopLayout
  static const QColor colors[]
      = {QColor(70, 130, 180),  QColor(180, 100, 60), QColor(80, 160, 80),
         QColor(180, 70, 140),  QColor(160, 160, 60), QColor(100, 180, 180),
         QColor(180, 130, 180), QColor(130, 130, 130)};

  for(int idx : indices)
  {
    auto* btn = new QPushButton(QString::number(idx));
    btn->setFixedSize(28, 28);
    btn->setCheckable(true);
    btn->setChecked(idx == m_selectedOutput);

    QColor c = colors[idx % 8];
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: white; border: 2px solid transparent; "
        "border-radius: 4px; font-weight: bold; }"
        "QPushButton:checked { border-color: white; }"
        "QPushButton:hover { background: %2; }")
                           .arg(c.name(), c.lighter(130).name()));

    connect(btn, &QPushButton::clicked, this, [this, idx] { selectOutput(idx); });
    m_outputButtonsLayout->addWidget(btn);
  }
  m_outputButtonsLayout->addStretch(1);
}

void WindowSettingsWidget::selectOutput(int index)
{
  if(m_syncing)
    return;
  m_syncing = true;

  m_selectedOutput = index;
  m_outputSectionsContainer->setVisible(index >= 0);

  // Select in input mapping canvas
  if(m_canvas && m_canvas->scene())
  {
    if(m_canvas->inWarpMode())
      m_canvas->exitWarpMode();
    m_canvas->scene()->clearSelection();
    for(auto* item : m_canvas->scene()->items())
    {
      if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
      {
        if(mi->outputIndex() == index)
        {
          mi->setSelected(true);
          break;
        }
      }
    }
  }

  // Select in desktop canvas
  if(m_desktopCanvas)
    m_desktopCanvas->selectItem(index);

  if(index >= 0)
    updatePropertiesFromSelection();

  rebuildOutputButtons();
  m_syncing = false;
}

void WindowSettingsWidget::syncPreview(bool syncPositions)
{
  if(m_preview && m_canvas)
  {
    m_preview->setSyncPositions(syncPositions);
    m_preview->syncToMappings(m_canvas->getMappings());
  }
}

void WindowSettingsWidget::syncDesktopCanvasFromMappingItem(int index)
{
  if(!m_desktopCanvas || !m_canvas || m_syncing || index < 0)
    return;

  for(auto* item : m_canvas->scene()->items())
  {
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
    {
      if(mi->outputIndex() == index)
      {
        m_desktopCanvas->updateItem(index, mi->windowPosition, mi->windowSize);

        // Also sync lock mode and aspect ratio to desktop canvas item
        for(auto* ditem : m_desktopCanvas->scene()->items())
        {
          if(auto* di = dynamic_cast<DesktopLayoutItem*>(ditem))
          {
            if(di->outputIndex() == index)
            {
              di->lockMode = mi->lockMode;
              // Compute aspect ratio from source rect
              auto sceneRect = mi->mapRectToScene(mi->rect());
              double srcW = sceneRect.width() / m_canvas->canvasWidth();
              double srcH = sceneRect.height() / m_canvas->canvasHeight();
              double pxW = srcW * m_inputWidth->value();
              double pxH = srcH * m_inputHeight->value();
              di->aspectRatio = pxW / qMax(1.0, pxH);
              di->applyLockedState();
              break;
            }
          }
        }
        return;
      }
    }
  }
}

Device::DeviceSettings WindowSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = WindowProtocolFactory::static_concreteKey();
  WindowSettings set;
  set.mode = (WindowMode)m_modeCombo->currentIndex();

  if(set.mode == WindowMode::MultiWindow && m_canvas)
  {
    // Apply any pending property changes before reading
    const_cast<WindowSettingsWidget*>(this)->applyPropertiesToSelection();
    set.outputs = m_canvas->getMappings();
  }

  if(m_inputWidth && m_inputHeight)
  {
    set.inputWidth = m_inputWidth->value();
    set.inputHeight = m_inputHeight->value();
  }

  s.deviceSpecificSettings = QVariant::fromValue(set);
  return s;
}

void WindowSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  if(settings.deviceSpecificSettings.canConvert<WindowSettings>())
  {
    const auto set = settings.deviceSpecificSettings.value<WindowSettings>();
    m_modeCombo->setCurrentIndex((int)set.mode);
    m_stack->setCurrentIndex((int)set.mode);

    if(m_inputWidth && m_inputHeight)
    {
      m_inputWidth->setValue(set.inputWidth);
      m_inputHeight->setValue(set.inputHeight);
    }

    if(this->m_modeCombo->currentIndex() == (int)WindowMode::MultiWindow)
    {
      if(auto doc = score::GUIAppContext().currentDocument())
      {
        auto& p = doc->plugin<Explorer::DeviceDocumentPlugin>();
        auto& dl = p.list();
        if(auto* self = dl.findDevice(settings.name))
        {
          safe_cast<WindowDevice*>(self)->disconnect();
        }
      }
    }

    if(set.mode == WindowMode::MultiWindow && m_canvas)
    {
      m_canvas->setMappings(set.outputs);
      if(m_desktopCanvas)
      {
        m_desktopCanvas->refreshScreens();
        m_desktopCanvas->setWindowItems(set.outputs);
      }
      rebuildOutputButtons();
      syncPreview(true);
    }
  }
}
}

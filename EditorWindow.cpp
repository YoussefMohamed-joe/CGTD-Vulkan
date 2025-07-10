#include "EditorWindow.h"
#include "VulkanWindow.h"
#include "VulkanRenderer.h"
#include "VPrimatives.h"

#include <QVulkanInstance>
#include <QVBoxLayout>
#include <QTreeWidgetItem>
#include <QDebug>
#include <QPainter>
#include <QMouseEvent>
#include <QFileDialog>
#include <QStandardPaths>
#include <QImage>
#include <QColorDialog>
#include <QTimer>
#include <QLabel>        
#include <QDoubleSpinBox>
#include <QPushButton> 

// ===================================================================
// == EyeIconDelegate Implementation
// ===================================================================
EyeIconDelegate::EyeIconDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

// Helper function to create fallback eye icons
QPixmap EyeIconDelegate::createFallbackEyeIcon(bool visible) const
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    if (visible) {
        // Draw visible eye (open eye)
        painter.setPen(QPen(Qt::black, 2));
        painter.setBrush(Qt::NoBrush);

        // Eye outline
        painter.drawEllipse(2, 6, 12, 4);

        // Pupil
        painter.setBrush(Qt::black);
        painter.drawEllipse(7, 7, 2, 2);
    }
    else {
        // Draw hidden eye (crossed out eye)
        painter.setPen(QPen(Qt::gray, 2));
        painter.setBrush(Qt::NoBrush);

        // Eye outline
        painter.drawEllipse(2, 6, 12, 4);

        // Cross out line
        painter.setPen(QPen(Qt::red, 2));
        painter.drawLine(2, 2, 14, 14);
    }

    return pixmap;
}

void EyeIconDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // Create a copy of the style options
    QStyleOptionViewItem newOption = option;

    // COMPLETELY remove all checkbox-related features
    newOption.features &= ~QStyleOptionViewItem::HasCheckIndicator;
    newOption.state &= ~QStyle::State_HasFocus;
    newOption.checkState = Qt::Unchecked;

    // Draw the item background and text normally
    QStyledItemDelegate::paint(painter, newOption, index);

    // Get visibility from custom role instead of checkState
    bool isVisible = index.data(Qt::UserRole).toBool();

    QPixmap eyeIcon;
    if (isVisible) {
        eyeIcon = QPixmap(":/icons/eye_visible.png");
        if (eyeIcon.isNull()) {
            eyeIcon = QPixmap(":/icons/icons/eye_visible.png");
        }
        if (eyeIcon.isNull()) {
            eyeIcon = createFallbackEyeIcon(true);
        }
    }
    else {
        eyeIcon = QPixmap(":/icons/eye_hidden.png");
        if (eyeIcon.isNull()) {
            eyeIcon = QPixmap(":/icons/icons/eye_hidden.png");
        }
        if (eyeIcon.isNull()) {
            eyeIcon = createFallbackEyeIcon(false);
        }
    }

    if (eyeIcon.isNull()) {
        qWarning() << "All eye icon loading attempts failed.";
        return;
    }

    // Draw the eye icon on the right side
    int iconSize = 16;
    int padding = 5;
    QRect iconRect(option.rect.right() - iconSize - padding,
        option.rect.y() + (option.rect.height() - iconSize) / 2,
        iconSize, iconSize);

    painter->drawPixmap(iconRect, eyeIcon);
}

bool EyeIconDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto mouseEvent = static_cast<QMouseEvent*>(event);

        int iconSize = 16;
        int padding = 5;
        QRect iconRect(option.rect.right() - iconSize - padding,
            option.rect.y() + (option.rect.height() - iconSize) / 2,
            iconSize, iconSize);

        if (iconRect.contains(mouseEvent->pos())) {
            // Toggle using custom role instead of checkState
            bool currentState = model->data(index, Qt::UserRole).toBool();
            model->setData(index, !currentState, Qt::UserRole);
            return true;
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

// ===================================================================
// == VulkanWidget (Main Window) Implementation
// ===================================================================
VulkanWidget::VulkanWidget(QWidget* parent, bool autoInit)
    : QMainWindow(parent), ui(new Ui::VulkanWidget) {
    ui->setupUi(this);

    // Configure the tree widget from the UI file
    ui->outlinerTree->setHeaderHidden(true);

    if (autoInit) {
        setupVulkanWindow();  //  Now it works
    }
    connectSignals();
    setupDesign();
    setupPropertiesPanel();

    // Instantiate and apply our custom delegate
    m_eyeDelegate = new EyeIconDelegate(this);
    ui->outlinerTree->setItemDelegate(m_eyeDelegate);

    m_overlayInitialized = false;
    m_overlayUpdateTimer = nullptr;

    ui->splitter_3->setSizes(QList<int>() << 1000 << 70);
    ui->splitter->setSizes(QList<int>() << 500 << 100);
    ui->splitter_2->setSizes(QList<int>() << 100 << 260);
    ui->splitter_4->setSizes(QList<int>() << 50 << 5000);
    ui->splitter_5->setSizes(QList<int>() << 5000 << 100);

    this->setWindowTitle("Fleura Engine");

    // 2. Set the window icon
    //    (This uses the same .qrc resource file as your other icons)
    this->setWindowIcon(QIcon(":icons/logo.png"));



}

VulkanWidget::~VulkanWidget() {
    // Clean up timer
    if (m_overlayUpdateTimer) {
        m_overlayUpdateTimer->stop();
        delete m_overlayUpdateTimer;
    }

    delete ui;
}

void VulkanWidget::connectSignals() {
    // Button connections for adding primitives
    connect(ui->cubeButton, &QPushButton::clicked, this, &VulkanWidget::onCubeClicked);
    connect(ui->sphereButton, &QPushButton::clicked, this, &VulkanWidget::onSphereClicked);
    connect(ui->cylinderButton, &QPushButton::clicked, this, &VulkanWidget::onCylinderClicked);
    connect(ui->pyramidButton, &QPushButton::clicked, this, &VulkanWidget::onPyramidClicked);

    // Clear button to clear all primitives
    connect(ui->clearButton, &QPushButton::clicked, this, &VulkanWidget::onClearClicked);

    // Connect the tree widget's signal to its slot
    connect(ui->outlinerTree, &QTreeWidget::itemChanged, this, &VulkanWidget::on_outlinerTree_itemChanged);

    // Toggle visibility for Grid
    connect(ui->toggleGridButton, &QPushButton::clicked, this, &VulkanWidget::onToggleGridClicked);

    // background color button
    connect(ui->actionChange_Grid_Background, &QAction::triggered, this, &VulkanWidget::onBackgroundColorClicked);
}

void VulkanWidget::onCubeClicked() {
    if (!m_vulkanWindow || !m_vulkanWindow->getRenderer()) return;

    auto primitive = VPrimatives::createCube();
    int id = m_vulkanWindow->getRenderer()->addPrimitive(primitive, "Cube");

    auto* item = new QTreeWidgetItem(ui->outlinerTree);
    item->setText(0, "Cube");

    // Remove ALL checkable flags
    item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);

    // Use custom data role instead of checkState (true = visible)
    item->setData(0, Qt::UserRole, true);

    // REMOVE this line completely:
    // item->setCheckState(0, Qt::Checked);

    m_primitiveItems[item] = id;
}

void VulkanWidget::onSphereClicked() {
    if (!m_vulkanWindow || !m_vulkanWindow->getRenderer()) return;

    auto primitive = VPrimatives::createSphere();
    int id = m_vulkanWindow->getRenderer()->addPrimitive(primitive, "Sphere");

    auto* item = new QTreeWidgetItem(ui->outlinerTree);
    item->setText(0, "Sphere");

    // Remove ALL checkable flags
    item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);

    // Use custom data role instead of checkState
    item->setData(0, Qt::UserRole, true);

    // REMOVE this line:
    // item->setCheckState(0, Qt::Checked);

    m_primitiveItems[item] = id;
}

void VulkanWidget::onCylinderClicked() {
    if (!m_vulkanWindow || !m_vulkanWindow->getRenderer()) return;

    auto primitive = VPrimatives::createCylinder();
    int id = m_vulkanWindow->getRenderer()->addPrimitive(primitive, "Cylinder");

    auto* item = new QTreeWidgetItem(ui->outlinerTree);
    item->setText(0, "Cylinder");

    // Remove ALL checkable flags
    item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);

    // Use custom data role instead of checkState
    item->setData(0, Qt::UserRole, true);

    // REMOVE this line:
    // item->setCheckState(0, Qt::Checked);

    m_primitiveItems[item] = id;
}

void VulkanWidget::onPyramidClicked() {
    if (!m_vulkanWindow || !m_vulkanWindow->getRenderer()) return;

    auto primitive = VPrimatives::createPyramid();
    int id = m_vulkanWindow->getRenderer()->addPrimitive(primitive, "Pyramid");

    auto* item = new QTreeWidgetItem(ui->outlinerTree);
    item->setText(0, "Pyramid");

    // Remove ALL checkable flags
    item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);

    // Use custom data role instead of checkState
    item->setData(0, Qt::UserRole, true);


    m_primitiveItems[item] = id;
}

void VulkanWidget::onClearClicked() {
    if (!m_vulkanWindow || !m_vulkanWindow->getRenderer()) return;
    m_vulkanWindow->getRenderer()->clearPrimitives();
    ui->outlinerTree->clear();
    m_primitiveItems.clear();
}


void VulkanWidget::on_outlinerTree_itemChanged(QTreeWidgetItem* item, int column) {
    if (column == 0 && m_primitiveItems.contains(item)) {
        int primitiveId = m_primitiveItems[item];
        // Get visibility from custom role instead of checkState
        bool isVisible = item->data(0, Qt::UserRole).toBool();

        if (m_vulkanWindow && m_vulkanWindow->getRenderer()) {
            m_vulkanWindow->getRenderer()->setPrimitiveVisibility(primitiveId, isVisible);
        }
    }
}

void VulkanWidget::onShowAllClicked() {
    for (int i = 0; i < ui->outlinerTree->topLevelItemCount(); ++i) {
        // Use custom role instead of checkState
        ui->outlinerTree->topLevelItem(i)->setData(0, Qt::UserRole, true);
    }
}

void VulkanWidget::onHideAllClicked() {
    for (int i = 0; i < ui->outlinerTree->topLevelItemCount(); ++i) {
        // Use custom role instead of checkState
        ui->outlinerTree->topLevelItem(i)->setData(0, Qt::UserRole, false);
    }
}

void VulkanWidget::onScreenshotClicked() {
    if (!m_vulkanWindow) return;
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString filePath = QFileDialog::getSaveFileName(this, "Save Screenshot",
        defaultPath + "/screenshot.png", "PNG Images (*.png)");
    if (filePath.isEmpty()) return;
    QImage image = m_vulkanWindow->grab();
    if (!image.save(filePath)) {
        qWarning() << "Failed to save screenshot";
    }
}

void VulkanWidget::onToggleGridClicked() {
    if (m_vulkanWindow && m_vulkanWindow->getRenderer()) {
        m_vulkanWindow->getRenderer()->toggleGrid();
    }
}

void VulkanWidget::onBackgroundColorClicked() {
    QColor color = QColorDialog::getColor(Qt::black, this, "Select Background Color");
    if (color.isValid()) {
        if (m_vulkanWindow && m_vulkanWindow->getRenderer()) {
            m_vulkanWindow->getRenderer()->setBackgroundColor(glm::vec4(color.redF(), color.greenF(), color.blueF(), color.alphaF()));
        }
    }
}

void VulkanWidget::setupVulkanWindow() {
    if (m_isVulkanInitialized)
        return;

    m_isVulkanInitialized = true;

    // STEP 1: Create Vulkan window
    m_vulkanWindow = new VulkanWindow();
    m_vulkanWindow->setVulkanInstance(createVulkanInstance());

    // STEP 2: Create Qt wrapper
    QWidget* vulkanContainerWidget = QWidget::createWindowContainer(m_vulkanWindow, ui->vulkanContainer);
    vulkanContainerWidget->setFocusPolicy(Qt::StrongFocus);
    vulkanContainerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vulkanContainerWidget->setMinimumSize(1, 1);

    // STEP 3: Remove layout and overlay from old layout
    if (auto* layout = ui->vulkanContainer->layout()) {
        layout->removeWidget(ui->overlayWidget);
        delete layout;
    }
    ui->overlayWidget->setParent(nullptr);

    // STEP 4: Insert Vulkan container into fresh layout
    auto* layout = new QGridLayout(ui->vulkanContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(vulkanContainerWidget, 0, 0);
    ui->vulkanContainer->setLayout(layout);

    // STEP 5: Initialize overlay system with proper timing
    initializeOverlaySystem();

    // STEP 6: Install event filter to handle focus changes
    this->installEventFilter(this);
    connect(m_vulkanWindow, &QWidget::destroyed, this, [this]() {
        if (ui->overlayWidget) {
            ui->overlayWidget->hide();
        }
        });
}

void VulkanWidget::initializeOverlaySystem() {
    // Create timer for deferred overlay setup (only once)
    if (!m_overlayUpdateTimer) {
        m_overlayUpdateTimer = new QTimer(this);
        m_overlayUpdateTimer->setSingleShot(true);
        m_overlayUpdateTimer->setInterval(100); // Reduced interval for faster initialization
        connect(m_overlayUpdateTimer, &QTimer::timeout, this, &VulkanWidget::setupOverlayWidget);
    }

    m_overlayUpdateTimer->start();
}

void VulkanWidget::setupOverlayWidget() {
    if (m_overlayInitialized) return;

    QWidget* overlay = ui->overlayWidget;
    if (!overlay) return;

    // Configure overlay widget (one-time setup)
    setupOverlayProperties(overlay);

    // Initialize button array and setup
    initializeButtonArray();

    // Position overlay and buttons immediately
    QTimer::singleShot(50, this, [this]() {
        updateOverlayGeometry();
        // Force a second update to ensure proper positioning
        QTimer::singleShot(100, this, [this]() {
            updateOverlayGeometry();
            });
        });

    m_overlayInitialized = true;
}

void VulkanWidget::setupOverlayProperties(QWidget* overlay) {
    // Make it float (not in layout)
    overlay->setParent(nullptr);
    overlay->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    overlay->setAttribute(Qt::WA_TranslucentBackground);
    overlay->setAttribute(Qt::WA_NoSystemBackground);
    overlay->setAttribute(Qt::WA_ShowWithoutActivating);

    // Allow it to grow
    overlay->setMinimumSize(1, 1);
    overlay->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    overlay->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Style the overlay and buttons (cached)
    static const QString overlayStyle = R"(
        QWidget {
            background-color: rgba(57, 62, 70, 160);
            border-radius: 8px;
        }
        QPushButton {
            background-color: #393E46;
            color: white;
            border: 1px solid #222;
            border-radius: 5px;
            padding: 5px 10px;
        }
        QPushButton:hover {
            background-color: #4E5862;
            border: 1px solid #5c5c5c;
        }
        QPushButton:pressed {
            background-color: #2C3138;
        }
    )";

    overlay->setStyleSheet(overlayStyle);

    // Initially hide the overlay
    overlay->hide();
}

void VulkanWidget::updateOverlayGeometry() {
    if (!m_overlayInitialized || !ui->overlayWidget) return;

    // Get render area dimensions
    const QSize renderSize = ui->vulkanContainer->size();
    const QPoint topLeft = ui->vulkanContainer->mapToGlobal(QPoint(0, 0));
    ui->vulkanContainer->installEventFilter(this);

    // Only show overlay if the main window is active and visible
    if (this->isActiveWindow() && this->isVisible() && !this->isMinimized() &&
        m_vulkanWindow && m_vulkanWindow->isVisible()) {
        // Update overlay geometry
        ui->overlayWidget->setGeometry(QRect(topLeft, renderSize));

        // Position buttons using cached array
        positionButtons(renderSize);

        // Show the overlay
        ui->overlayWidget->show();
        ui->overlayWidget->raise();
    }
    else {
        // Hide the overlay when window is not active
        ui->overlayWidget->hide();
    }
}

void VulkanWidget::positionButtons(const QSize& renderSize) {
    const int x = renderSize.width() - BUTTON_WIDTH - SPACING;

    for (int i = 0; i < BUTTON_COUNT; ++i) {
        if (m_overlayButtons[i]) {
            const int y = TOP_MARGIN + i * (BUTTON_HEIGHT + SPACING);
            m_overlayButtons[i]->move(x, y);
            m_overlayButtons[i]->raise();
            m_overlayButtons[i]->show();
        }
    }
}

void VulkanWidget::focusOutEvent(QFocusEvent* event) {
    // Hide overlay when window loses focus
    if (ui->overlayWidget) {
        ui->overlayWidget->hide();
    }
    QMainWindow::focusOutEvent(event);
}

void VulkanWidget::focusInEvent(QFocusEvent* event) {
    // Show overlay when window gains focus
    if (m_overlayInitialized) {
        QTimer::singleShot(10, this, [this]() {
            updateOverlayGeometry();
            });
    }
    QMainWindow::focusInEvent(event);
}

void VulkanWidget::closeEvent(QCloseEvent* event) {
    if (ui->overlayWidget) {
        ui->overlayWidget->close();
    }
    QMainWindow::closeEvent(event);
}


void VulkanWidget::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);

    // Update overlay geometry with a small delay to ensure proper positioning
    if (m_overlayInitialized) {
        QTimer::singleShot(10, this, [this]() {
            updateOverlayGeometry();
            });
    }
}





void VulkanWidget::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange && m_overlayInitialized) {
        if (isMinimized() || !isVisible()) {
            ui->overlayWidget->hide();
        }
        else {
            QTimer::singleShot(50, this, [this]() {
                updateOverlayGeometry();
                });
        }
    }
    else if (event->type() == QEvent::ActivationChange) {
        // Handle window activation/deactivation
        if (m_overlayInitialized) {
            QTimer::singleShot(10, this, [this]() {
                updateOverlayGeometry();
                });
        }
    }
}

void VulkanWidget::moveEvent(QMoveEvent* event) {
    QMainWindow::moveEvent(event);

    // Update overlay position when window moves
    if (m_overlayInitialized) {
        QTimer::singleShot(10, this, [this]() {
            updateOverlayGeometry();
            });
    }
}

void VulkanWidget::initializeButtonArray() {
    // Initialize all buttons to nullptr first
    m_overlayButtons.fill(nullptr);

    // Get the overlay widget
    QWidget* overlay = ui->overlayWidget;
    if (!overlay) {
        qWarning() << "Overlay widget not found, cannot initialize buttons";
        return;
    }

    // Create or find the buttons (assuming they exist in the overlay widget)
    // If buttons don't exist, create them
    QList<QPushButton*> buttons = overlay->findChildren<QPushButton*>();

    // If we don't have enough buttons, create them
    if (buttons.size() < BUTTON_COUNT) {
        // Create missing buttons
        for (int i = buttons.size(); i < BUTTON_COUNT; ++i) {
            QString buttonText;
            switch (i) {
            case 0: buttonText = "Screenshot"; break;
            case 1: buttonText = "Show All"; break;
            case 2: buttonText = "Hide All"; break;
            case 3: buttonText = "Toggle Grid"; break;
            default: buttonText = QString("Button %1").arg(i + 1); break;
            }

            QPushButton* button = new QPushButton(buttonText, overlay);
            button->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);
            buttons.append(button);

            // Connect button signals
            switch (i) {
            case 0: connect(button, &QPushButton::clicked, this, &VulkanWidget::onScreenshotClicked); break;
            case 1: connect(button, &QPushButton::clicked, this, &VulkanWidget::onShowAllClicked); break;
            case 2: connect(button, &QPushButton::clicked, this, &VulkanWidget::onHideAllClicked); break;
            case 3: connect(button, &QPushButton::clicked, this, &VulkanWidget::onToggleGridClicked); break;
            }
        }
    }

    // Populate the array with the first BUTTON_COUNT buttons
    for (int i = 0; i < BUTTON_COUNT && i < buttons.size(); ++i) {
        m_overlayButtons[i] = buttons[i];
        // Ensure button has correct size
        if (m_overlayButtons[i]) {
            m_overlayButtons[i]->setFixedSize(BUTTON_WIDTH, BUTTON_HEIGHT);
        }
    }
}

bool VulkanWidget::eventFilter(QObject* watched, QEvent* event) {
    if (m_overlayInitialized) {
        // === Window-level overlay management ===
        if (watched == this) {
            switch (event->type()) {
            case QEvent::WindowActivate:
                QTimer::singleShot(10, this, [this]() {
                    updateOverlayGeometry();
                    });
                break;
            case QEvent::WindowDeactivate:
                if (ui->overlayWidget) {
                    ui->overlayWidget->hide();
                }
                break;
            case QEvent::Show:
                if (this->isActiveWindow()) {
                    QTimer::singleShot(50, this, [this]() {
                        updateOverlayGeometry();
                        });
                }
                break;
            case QEvent::Hide:
                if (ui->overlayWidget) {
                    ui->overlayWidget->hide();
                }
                break;
            default:
                break;
            }
        }

        // === VulkanContainer-specific: Handle splitter resize responsiveness ===
        if (watched == ui->vulkanContainer) {
            if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
                QSize size = ui->vulkanContainer->size();
                if (size.width() > 50 && size.height() > 50) {
                    QTimer::singleShot(10, this, [this]() {
                        updateOverlayGeometry();
                        });
                }
                else {
                    ui->overlayWidget->hide();
                }
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}


QVulkanInstance* VulkanWidget::createVulkanInstance() {
    auto* instance = new QVulkanInstance();
#ifndef NDEBUG
    instance->setLayers({ "VK_LAYER_KHRONOS_validation" });
#endif
    if (!instance->create()) {
        qFatal("Failed to create Vulkan instance: %d", instance->errorCode());
    }
    return instance;
}



//========================================================
void VulkanWidget::setupDesign() {

    // Remove all margins and spacing from central widget
    QLayout* layout = ui->centralwidget->layout();
    if (layout) {
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }
    QLayout* Outliner = ui->Outliner->layout();
    if (Outliner) {
        Outliner->setContentsMargins(0, 3, 0, 0);
        Outliner->setSpacing(0);
    }
    QLayout* tab_3 = ui->tab_3->layout();
    if (tab_3) {
        tab_3->setContentsMargins(0, 3, 0, 0);
        tab_3->setSpacing(0);
    }
    QLayout* tab_7 = ui->tab_7->layout();
    if (tab_7) {
        tab_7->setContentsMargins(0, 3, 0, 0);
        tab_7->setSpacing(0);
    }

    // Remove margins for all tab widgets
    ui->tabWidget->setContentsMargins(0, 0, 0, 0);
    ui->tabWidget_2->setContentsMargins(0, 0, 0, 0);
    ui->tabWidget_3->setContentsMargins(0, 0, 0, 0);
    ui->tabWidget_4->setContentsMargins(0, 0, 0, 0);

    // Set splitter handle width to match design (very thin)
    ui->splitter->setHandleWidth(1);
    ui->splitter_2->setHandleWidth(3);
    ui->splitter_3->setHandleWidth(3);
    ui->splitter_4->setHandleWidth(1);
    ui->splitter_5->setHandleWidth(2);

    // Set splitter colors to match the interface
    ui->splitter->setStyleSheet("QSplitter::handle { background-color: #222831; }");
    ui->splitter_2->setStyleSheet("QSplitter::handle { background-color: #222831; }");
    ui->splitter_3->setStyleSheet("QSplitter::handle { background-color: #222831; }");
    ui->splitter_4->setStyleSheet("QSplitter::handle { background-color: #222831; }");
    ui->splitter_5->setStyleSheet("QSplitter::handle { background-color: #222831; }");

    // Remove tab bar margins
    ui->tabWidget->tabBar()->setContentsMargins(0, 0, 0, 0);
    ui->tabWidget_2->tabBar()->setContentsMargins(0, 0, 0, 0);
    ui->tabWidget_3->tabBar()->setContentsMargins(0, 0, 0, 0);
    ui->tabWidget_4->tabBar()->setContentsMargins(0, 0, 0, 0);

    // Hide or remove footer if it exists
    if (ui->statusbar) {
        ui->statusbar->hide();
    }

    // Apply stylesheets in parts to avoid string length issues
    applyBaseStyles();
    applyLayoutStyles();
    applyInputStyles();
    applyNavigationStyles();

    // Special styling for tabWidget_3 to be blue
    ui->tabWidget_3->setStyleSheet("QTabWidget { background-color: #222831; } QTabWidget::pane { background-color: #4a9eff; }");

    // Force update
    this->update();
    this->repaint();

    //menuBar()->setStyleSheet("QMenuBar { icon-size: 20px; }");         // Fixed height
    menuBar()->setStyleSheet(
        "QMenuBar { "
        "} "
        "QMenuBar::item { "
        "   padding-left: 37px; "    // Increase left padding
        "   padding-right: 37px; "   // Increase right padding
        "   padding-top: 6px; "      // Optional: vertical padding
        "   padding-bottom: 6px; "   // Optional: vertical padding
        "}"
    );

    QString treeCheckboxHideCSS = R"(
        QTreeWidget::indicator {
            width: 0px;
            height: 0px;
            margin: 0px;
            padding: 0px;
            border: none;
            background: none;
        }
        
        QTreeWidget::indicator:unchecked,
        QTreeWidget::indicator:checked {
            width: 0px;
            height: 0px;
            margin: 0px;
            padding: 0px;
            border: none;
            background: none;
            image: none;
        }
    )";

    ui->outlinerTree->setStyleSheet(ui->outlinerTree->styleSheet() + treeCheckboxHideCSS);
    // page 7 and 8 no margin 
    this->setContentsMargins(0, 0, 0, 0);

    // Remove margins from central widget if you have one
    if (centralWidget()) {
        centralWidget()->setContentsMargins(0, 0, 0, 0);
        if (centralWidget()->layout()) {
            centralWidget()->layout()->setContentsMargins(0, 0, 0, 0);
            centralWidget()->layout()->setSpacing(0);
        }
    }

    // Configure stackedWidget
    ui->stackedWidget->setContentsMargins(0, 0, 0, 0);

    // Configure each page
    ui->page_7->setContentsMargins(0, 0, 0, 0);
    ui->page_8->setContentsMargins(0, 0, 0, 0);

    // Configure vulkanContainer
    ui->vulkanContainer->setContentsMargins(0, 0, 0, 0);

    // Remove margins from all layouts
    QList<QLayout*> layouts = {
        ui->page_7->layout(),
        ui->page_8->layout(),
        ui->vulkanContainer->layout()
    };

    for (QLayout* layout : layouts) {
        if (layout) {
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
        }
    }
}

void VulkanWidget::applyBaseStyles() {
    this->setStyleSheet(R"(
        /* --------- Base Application --------- */
        QWidget {
            background-color: #222831;
            color: #e6e6e6;
            font-family: "Segoe UI", Arial, sans-serif;
            font-size: 11px;
            margin: 0px;
            padding: 0px;
        }
        
        QMainWindow {
            background-color: #393e46;
            border: none;
            margin: 0px;
            padding: 0px;
        }
        
        QMainWindow::separator {
            background-color: #1e1e1e;
            width: 1px;
            height: 1px;
            margin: 0px;
            padding: 0px;
        }
        
        /* --------- Central Widget (3D Viewport) --------- */
        QWidget#centralwidget {
            background-color: #2d3035;
            margin: 0px;
            padding: 0px;
            border: none;
        }
        
        /* --------- Dock Widgets (Side Panels) --------- */
        QDockWidget {
            background-color: #393e46;
            border: none;
            margin: 0px;
            padding: 0px;
            titlebar-close-icon: none;
            titlebar-normal-icon: none;
        }
        
        QDockWidget::title {
            background-color: #393e46;
            color: #e6e6e6;
            padding: 8px 12px;
            border: none;
            margin: 0px;
            font-size: 11px;
            font-weight: normal;
        }
        
        QDockWidget::close-button, QDockWidget::float-button {
            background-color: transparent;
            border: none;
            padding: 0px;
            margin: 0px;
        }
        
        /* --------- Frames and Group Boxes --------- */
        QFrame, QGroupBox {
            background-color: #393e46;
            border: none;
            margin: 0px;
            padding: 0px;
        }
        
        /* --------- Scrollbars --------- */
        QScrollBar:vertical {
            background-color: #393e46;
            width: 14px;
            margin: 0px;
            border: none;
            padding: 0px;
        }
        
        QScrollBar:horizontal {
            background-color: #393e46;
            height: 14px;
            margin: 0px;
            border: none;
            padding: 0px;
        }
        
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background-color: #505562;
            border: none;
            border-radius: 2px;
            margin: 2px;
            padding: 0px;
        }
        
        QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover {
            background-color: #606872;
        }
        
        QScrollBar::add-line, QScrollBar::sub-line {
            background: none;
            border: none;
            width: 0px;
            height: 0px;
            margin: 0px;
            padding: 0px;
        }
        
        QScrollBar::add-page, QScrollBar::sub-page {
            background: none;
            border: none;
            margin: 0px;
            padding: 0px;
        }
    )");
}

void VulkanWidget::applyLayoutStyles() {
    QString layoutStyles = R"(
        /* --------- Splitters --------- */
        QSplitter {
            background-color: #393e46;
            border: none;
            margin: 0px;
            padding: 0px;
        }
        
        QSplitter::handle {
            background-color: #4a9eff;
            border: none;
            margin: 0px;
            padding: 0px;
        }
        
        QSplitter::handle:horizontal {
            width: 1px;
            margin: 0px;
            padding: 0px;
        }
        
        QSplitter::handle:vertical {
            height: 1px;
            margin: 0px;
            padding: 0px;
        }

         /* --------- stacked Widgets --------- */
        QStackedWidget {
            border: none;
            margin: 0px;
            padding: 0px;
            spacing: 0px;
        }
        QStackedWidget > QWidget {
            border: none;
            margin: 0px;
            padding: 0px;
        }

        QStackedWidget QWidget {
            border: none;
            margin: 0px;
            padding: 0px;
        }
        /* --------- Tab Widgets --------- */
        QTabWidget {
            background-color: #393e46;
            border: none;
            margin: 0px;
            padding: 0px;
        }
        
        QTabWidget::pane {
            background-color: #393e46;
            border: none;
            margin: 0px;
            padding: 0px;
            top: 0px;
        }
        
        QTabWidget::tab-bar {
            alignment: left;
            left: 0px;
            margin: 0px;
            padding: 0px;
        }
        
        QTabBar {
            background-color: #393e46;
            border: none;
            margin: 0px;
            padding: 0px;
        }
        
        QTabBar::tab {
            background-color: #393e46;
            color: #e6e6e6;
            padding: 8px 16px;
            border: none;
            margin: 0px;
            border-top-left-radius: 0px;
            border-top-right-radius: 0px;
            min-width: 60px;
            font-size: 11px;
        }

        QTabBar::tab:first {
            margin-left: 0px;
        }
        
        QTabBar::tab:selected {
            background-color: #4a9eff;
            color: #ffffff;
            font-weight: normal;
        }
        
        QTabBar::tab:hover:!selected {
            background-color: #4a525a;
            color: #ffffff;
        }
        
        QTabBar::tab:!selected {
            background-color: #393e46;
            color: #e6e6e6;
        }
        

        
        /* --------- List & Table Widgets --------- */
        QListWidget, QTableWidget {
            background-color: #393e46;
            color: #e6e6e6;
            border: none;
            outline: none;
            margin: 0px;
            padding: 4px;
            font-size: 11px;
        }
        
        QListWidget::item, QTableWidget::item {
            background-color: transparent;
            color: #e6e6e6;
            padding: 3px 6px;
            border: none;
            margin: 0px;
        }

        QListWidget::item { height: 20px; }
        
        QListWidget::item:selected, QTableWidget::item:selected {
            background-color: #4a9eff;
            color: #ffffff;
        }
        
        QListWidget::item:hover:!selected, QTableWidget::item:hover:!selected {
            background-color: #4a525a;
            color: #ffffff;
        }
        
        /* --------- Headers --------- */
        QHeaderView {
            background-color: #393e46;
            border: none;
            margin: 0px;
            padding: 0px;
        }
        
        QHeaderView::section {
            background-color: #393e46;
            color: #e6e6e6;
            padding: 6px 8px;
            border: none;
            border-right: 1px solid #1e1e1e;
            margin: 0px;
            font-size: 11px;
        }
        
        QHeaderView::section:hover {
            background-color: #4a525a;
        }
    )";
    this->setStyleSheet(this->styleSheet() + layoutStyles);
}


void VulkanWidget::applyInputStyles() {
    QString inputStyles = R"(
        /* --------- Buttons --------- */
        QPushButton {
            background-color: #222831;
            color: #e6e6e6;
            border: 1px solid #222831;
            padding: 6px 12px;
            border-radius: 2px;
            font-size: 11px;
            margin: 1px;
        }
        
        QPushButton:hover {
            background-color: #4E5862;
            border: 1px solid #5c5c5c;
        }
        QPushButton:pressed {
            background-color: #2C3138;
        }
        
        /* --------- Text Fields --------- */
        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: #2d3035;
            color: #e6e6e6;
            border: 1px solid #1e1e1e;
            padding: 6px 8px;
            border-radius: 2px;
            font-size: 11px;
            margin: 0px;
        }
        
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border-color: #4a9eff;
            background-color: #2d3035;
        }
        
        /* --------- Combo Boxes --------- */
        QComboBox {
            background-color: #393e46;
            color: #e6e6e6;
            border: 1px solid #1e1e1e;
            padding: 6px 8px;
            border-radius: 2px;
            font-size: 11px;
            margin: 0px;
        }
        
        QComboBox:hover {
            border-color: #4a9eff;
            background-color: #4a525a;
        }
        
        QComboBox::drop-down {
            border: none;
            width: 20px;
            margin: 0px;
            padding: 0px;
        }
        
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 4px solid #e6e6e6;
            margin-right: 4px;
        }
        
        QComboBox QAbstractItemView {
            background-color: #393e46;
            color: #e6e6e6;
            border: 1px solid #1e1e1e;
            selection-background-color: #4a9eff;
            selection-color: #ffffff;
            outline: none;
            margin: 0px;
            padding: 0px;
        }
        
        /* --------- Spin Boxes --------- */
        QSpinBox, QDoubleSpinBox {
            background-color: #393e46;
            color: #e6e6e6;
            border: 1px solid #1e1e1e;
            padding: 6px 8px;
            border-radius: 2px;
            font-size: 11px;
            margin: 0px;
        }
        
        QSpinBox:focus, QDoubleSpinBox:focus {
            border-color: #4a9eff;
        }
        
        QSpinBox::up-button, QSpinBox::down-button,
        QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
            background-color: #4a525a;
            border: none;
            width: 16px;
            margin: 0px;
            padding: 0px;
        }
        
        QSpinBox::up-button:hover, QSpinBox::down-button:hover,
        QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover {
            background-color: #4a9eff;
        }
        
        /* --------- Progress Bars --------- */
        QProgressBar {
            background-color: #393e46;
            color: #e6e6e6;
            border: 1px solid #1e1e1e;
            border-radius: 2px;
            text-align: center;
            font-size: 11px;
            margin: 0px;
            padding: 0px;
        }
        
        QProgressBar::chunk {
            background-color: #4a9eff;
            border-radius: 2px;
            margin: 0px;
            padding: 0px;
        }
        
        /* --------- Sliders --------- */
        QSlider::groove:horizontal {
            background-color: #1e1e1e;
            height: 6px;
            border-radius: 3px;
            margin: 0px;
            padding: 0px;
        }
        
        QSlider::handle:horizontal {
            background-color: #4a9eff;
            border: none;
            width: 16px;
            height: 16px;
            border-radius: 8px;
            margin: -5px 0px;
            padding: 0px;
        }
        
        QSlider::handle:horizontal:hover {
            background-color: #5ab0ff;
        }
        
        QSlider::groove:vertical {
            background-color: #1e1e1e;
            width: 6px;
            border-radius: 3px;
            margin: 0px;
            padding: 0px;
        }
        
        QSlider::handle:vertical {
            background-color: #4a9eff;
            border: none;
            width: 16px;
            height: 16px;
            border-radius: 8px;
            margin: 0px -5px;
            padding: 0px;
        }
        
        QSlider::handle:vertical:hover {
            background-color: #5ab0ff;
        }
    )";
    this->setStyleSheet(this->styleSheet() + inputStyles);
}

void VulkanWidget::applyNavigationStyles() {
    QString navigationStyles = R"(
        /* --------- Menu Bar --------- */
        QMenuBar {
            background-color: #4a525a;
            color: #e6e6e6;
            border: none;
            margin: 0px;
            padding: 0px;
            font-size: 11px;
        }
        
        QMenuBar::item {
            background-color: transparent;
            color: #e6e6e6;
            padding: 8px 12px;
            margin: 0px;
            border: none;
        }
        
        QMenuBar::item:selected {
            background-color: #4a9eff;
            color: #ffffff;
        }
        
        QMenuBar::item:pressed {
            background-color: #4a9eff;
            color: #ffffff;
        }
        
        /* --------- Menus --------- */
        QMenu {
            background-color: #393e46;
            color: #e6e6e6;
            border: 1px solid #1e1e1e;
            margin: 0px;
            padding: 4px;
            font-size: 11px;
        }
        
        QMenu::item {
            background-color: transparent;
            color: #e6e6e6;
            padding: 6px 20px;
            margin: 0px;
            border: none;
        }
        
        QMenu::item:selected {
            background-color: #4E5862;
            color: #ffffff;
        }
        
        QMenu::separator {
            height: 1px;
            background-color: #1e1e1e;
            margin: 4px 0px;
        }
        
        /* --------- Tool Bar --------- */
        QToolBar {
            background-color: #4a525a;
            border: none;
            margin: 0px;
            padding: 4px;
            spacing: 2px;
            font-size: 11px;
        }
        
        QToolButton {
            background-color: transparent;
            color: #e6e6e6;
            border: none;
            padding: 6px;
            margin: 0px;
            border-radius: 2px;
            min-width: 24px;
            min-height: 24px;
        }
        
        QToolButton:hover {
            background-color: #4a525a;
            color: #ffffff;
        }
        
        QToolButton:pressed {
            background-color: #4a9eff;
            color: #ffffff;
        }
        
        /* --------- Status Bar (Hidden) --------- */
        QStatusBar {
            background-color: #393e46;
            color: #e6e6e6;
            border: none;
            margin: 0px;
            padding: 0px;
            font-size: 11px;
            max-height: 0px;
            min-height: 0px;
        }
        
        QStatusBar::item {
            border: none;
            margin: 0px;
            padding: 0px;
        }

    )";

    this->setStyleSheet(this->styleSheet() + navigationStyles);
}


void VulkanWidget::keyPressEvent(QKeyEvent* event) {
    if (m_vulkanWindow && m_vulkanWindow->getRenderer()) {
        m_vulkanWindow->getRenderer()->setKeyPressed(event->key(), true);
    }
    QMainWindow::keyPressEvent(event);
}

void VulkanWidget::keyReleaseEvent(QKeyEvent* event) {
    if (m_vulkanWindow && m_vulkanWindow->getRenderer()) {
        m_vulkanWindow->getRenderer()->setKeyPressed(event->key(), false);
    }
    QMainWindow::keyReleaseEvent(event);
}



void VulkanWidget::setupPropertiesPanel() {
    QTreeWidget* tree = ui->propertiesTree;
    tree->setColumnCount(2);
    tree->setIndentation(14);
    tree->setColumnWidth(0, 140);
    tree->header()->setStretchLastSection(true);
    tree->setRootIsDecorated(true); // Allow expand/collapse arrows

    QFont sectionFont("Segoe UI", 9, QFont::Bold);

    // Helper to create a row with a label, spinbox, and reset button
    auto createAxisRow = [&](const QString& axisName, QDoubleSpinBox*& spin,
        const QObject* receiver, void (VulkanWidget::* resetSlot)()) -> QWidget* {
            QWidget* axisRow = new QWidget();
            QHBoxLayout* axisLayout = new QHBoxLayout(axisRow);
            axisLayout->setContentsMargins(0, 0, 0, 0);
            axisLayout->setSpacing(5);

            QLabel* label = new QLabel(axisName);
            label->setObjectName("coordLabel");

            spin = new QDoubleSpinBox();
            spin->setDecimals(2);
            spin->setRange(-999999.0, 999999.0);
            spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
            spin->setMinimumWidth(55);
            spin->setValue(0.0);

            QPushButton* resetBtn = new QPushButton();
            resetBtn->setIcon(QIcon(":/icons/reset.png"));
            resetBtn->setFixedSize(18, 18);
            resetBtn->setFlat(true);
            resetBtn->setCursor(Qt::PointingHandCursor);
            QObject::connect(resetBtn, &QPushButton::clicked, receiver, resetSlot);

            axisLayout->addWidget(label);
            axisLayout->addWidget(spin);
            axisLayout->addWidget(resetBtn);
            axisLayout->addStretch();

            return axisRow;
        };

    // Helper to create the full control widget for Translate/Rotate/Scale
    auto createVectorControl = [&](QDoubleSpinBox*& xSpin, QDoubleSpinBox*& ySpin, QDoubleSpinBox*& zSpin,
        const QObject* receiver,
        void (VulkanWidget::* resetX)(), void (VulkanWidget::* resetY)(), void (VulkanWidget::* resetZ)()) -> QWidget* {
            QWidget* container = new QWidget();
            QVBoxLayout* layout = new QVBoxLayout(container);
            layout->setContentsMargins(8, 4, 8, 4);
            layout->setSpacing(6);

            layout->addWidget(createAxisRow("X", xSpin, receiver, resetX));
            layout->addWidget(createAxisRow("Y", ySpin, receiver, resetY));
            layout->addWidget(createAxisRow("Z", zSpin, receiver, resetZ));

            return container;
        };

    // Top-level section
    QTreeWidgetItem* transformItem = new QTreeWidgetItem(tree);
    transformItem->setText(0, "Transform");
    transformItem->setFont(0, sectionFont);
    transformItem->setIcon(0, QIcon(":/icons/transform.png"));
    transformItem->setFlags(Qt::ItemIsEnabled); // Not expandable

    // --- Translate ---
    {
        QTreeWidgetItem* translateItem = new QTreeWidgetItem(transformItem);
        translateItem->setText(0, "Translate");
        translateItem->setFont(0, sectionFont);
        translateItem->setIcon(0, QIcon(":/icons/translate2.png"));
        translateItem->setFlags(Qt::ItemIsEnabled);

        QTreeWidgetItem* inputItem = new QTreeWidgetItem(translateItem);
        inputItem->setFlags(Qt::ItemIsEnabled);

        QWidget* widget = createVectorControl(
            m_translateXSpin, m_translateYSpin, m_translateZSpin,
            this,
            &VulkanWidget::onResetTranslateX,
            &VulkanWidget::onResetTranslateY,
            &VulkanWidget::onResetTranslateZ
        );

        tree->setItemWidget(inputItem, 1, widget);
    }

    // --- Rotate ---
    {
        QTreeWidgetItem* rotateItem = new QTreeWidgetItem(transformItem);
        rotateItem->setText(0, "Rotate (Deg)");
        rotateItem->setFont(0, sectionFont);
        rotateItem->setIcon(0, QIcon(":/icons/rotate2.png"));
        rotateItem->setFlags(Qt::ItemIsEnabled);

        QTreeWidgetItem* inputItem = new QTreeWidgetItem(rotateItem);
        inputItem->setFlags(Qt::ItemIsEnabled);

        QWidget* widget = createVectorControl(
            m_rotateXSpin, m_rotateYSpin, m_rotateZSpin,
            this,
            &VulkanWidget::onResetRotateX,
            &VulkanWidget::onResetRotateY,
            &VulkanWidget::onResetRotateZ
        );

        m_rotateXSpin->setRange(-360, 360);
        m_rotateYSpin->setRange(-360, 360);
        m_rotateZSpin->setRange(-360, 360);
        m_rotateXSpin->setDecimals(1);
        m_rotateYSpin->setDecimals(1);
        m_rotateZSpin->setDecimals(1);

        tree->setItemWidget(inputItem, 1, widget);
    }

    // --- Scale ---
    {
        QTreeWidgetItem* scaleItem = new QTreeWidgetItem(transformItem);
        scaleItem->setText(0, "Scale");
        scaleItem->setFont(0, sectionFont);
        scaleItem->setIcon(0, QIcon(":/icons/scale2.png"));
        scaleItem->setFlags(Qt::ItemIsEnabled);

        QTreeWidgetItem* inputItem = new QTreeWidgetItem(scaleItem);
        inputItem->setFlags(Qt::ItemIsEnabled);

        QWidget* widget = createVectorControl(
            m_scaleXSpin, m_scaleYSpin, m_scaleZSpin,
            this,
            &VulkanWidget::onResetScaleX,
            &VulkanWidget::onResetScaleY,
            &VulkanWidget::onResetScaleZ
        );

        m_scaleXSpin->setRange(-999999, 999999);
        m_scaleYSpin->setRange(-999999, 999999);
        m_scaleZSpin->setRange(-999999, 999999);
        m_scaleXSpin->setValue(1.0);
        m_scaleYSpin->setValue(1.0);
        m_scaleZSpin->setValue(1.0);

        tree->setItemWidget(inputItem, 1, widget);
    }

    // Connect value change signals
    connect(m_translateXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VulkanWidget::onTranslateSpinChanged);
    connect(m_translateYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VulkanWidget::onTranslateSpinChanged);
    connect(m_translateZSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VulkanWidget::onTranslateSpinChanged);

    connect(m_rotateXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VulkanWidget::onRotateSpinChanged);
    connect(m_rotateYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VulkanWidget::onRotateSpinChanged);
    connect(m_rotateZSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VulkanWidget::onRotateSpinChanged);

    connect(m_scaleXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VulkanWidget::onScaleSpinChanged);
    connect(m_scaleYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VulkanWidget::onScaleSpinChanged);
    connect(m_scaleZSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VulkanWidget::onScaleSpinChanged);

    // Styling
    QString style = R"(
        QTreeWidget#propertiesTree {
            background-color: #393E46;
            color: #e6e6e6;
            border: none;
            selection-background-color: transparent;
        }
        QTreeWidget::item {
            padding: 3px;
        }
        QTreeWidget::branch {
            background: transparent;
            width: 0px;
            image: none;
        }
        QTreeWidget::item:selected {
            background-color: transparent;
            color: #e6e6e6;
        }
        QDoubleSpinBox {
            background-color: #222831;
            color: #e6e6e6;
            border: 1px solid #3c3f44;
            border-radius: 3px;
            padding: 2px 4px;
        }
        QDoubleSpinBox:focus {
            border: 1px solid #4a9eff;
        }
        QLabel#coordLabel {
            color: #e6e6e6;
            background-color: #222831;
            padding: 2px 6px;
            border-radius: 3px;
            font-weight: bold;
        }
        QPushButton {
            background-color: transparent;
            border: none;
        }
        QPushButton:hover {
            background-color: #4a525a;
            border-radius: 3px;
        }
QTreeWidget#propertiesTree::branch:has-children:!has-siblings:closed,
QTreeWidget#propertiesTree::branch:closed:has-children:has-siblings {
    image: url(:/icons/arrow-right.png);
}

QTreeWidget#propertiesTree::branch:open:has-children:!has-siblings,
QTreeWidget#propertiesTree::branch:open:has-children:has-siblings {
    image: url(:/icons/arrow-down.png);
}
QTreeWidget::branch {
    padding-left: 3px;
}
    )";
    tree->setStyleSheet(style);

    tree->expandAll();
}






void VulkanWidget::updateTransformPanel(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale) {
    // Block signals to prevent infinite loops when updating UI from code
    QSignalBlocker xBlocker(m_translateXSpin), yBlocker(m_translateYSpin), zBlocker(m_translateZSpin);
    QSignalBlocker rxBlocker(m_rotateXSpin), ryBlocker(m_rotateYSpin), rzBlocker(m_rotateZSpin);
    QSignalBlocker sxBlocker(m_scaleXSpin), syBlocker(m_scaleYSpin), szBlocker(m_scaleZSpin);

    m_translateXSpin->setValue(position.x);
    m_translateYSpin->setValue(position.y);
    m_translateZSpin->setValue(position.z);

    m_rotateXSpin->setValue(rotation.x);
    m_rotateYSpin->setValue(rotation.y);
    m_rotateZSpin->setValue(rotation.z);

    m_scaleXSpin->setValue(scale.x);
    m_scaleYSpin->setValue(scale.y);
    m_scaleZSpin->setValue(scale.z);
}

void VulkanWidget::onTranslateSpinChanged() {
    glm::vec3 values(m_translateXSpin->value(), m_translateYSpin->value(), m_translateZSpin->value());
    emit transformValuesChanged(Translate, values);
}

void VulkanWidget::onRotateSpinChanged() {
    glm::vec3 values(m_rotateXSpin->value(), m_rotateYSpin->value(), m_rotateZSpin->value());
    emit transformValuesChanged(Rotate, values);
}

void VulkanWidget::onScaleSpinChanged() {
    glm::vec3 values(m_scaleXSpin->value(), m_scaleYSpin->value(), m_scaleZSpin->value());
    emit transformValuesChanged(Scale, values);
}

// --- Implementation for Reset Button Slots ---

void VulkanWidget::onResetTranslate() {
    // Block signals while setting values
    QSignalBlocker xBlocker(m_translateXSpin), yBlocker(m_translateYSpin), zBlocker(m_translateZSpin);
    m_translateXSpin->setValue(0.0);
    m_translateYSpin->setValue(0.0);
    m_translateZSpin->setValue(0.0);

    // Manually trigger the changed signal after resetting
    onTranslateSpinChanged();
}

void VulkanWidget::onResetRotate() {
    QSignalBlocker xBlocker(m_rotateXSpin), yBlocker(m_rotateYSpin), zBlocker(m_rotateZSpin);
    m_rotateXSpin->setValue(0.0);
    m_rotateYSpin->setValue(0.0);
    m_rotateZSpin->setValue(0.0);
    onRotateSpinChanged();
}

void VulkanWidget::onResetScale() {
    QSignalBlocker xBlocker(m_scaleXSpin), yBlocker(m_scaleYSpin), zBlocker(m_scaleZSpin);
    m_scaleXSpin->setValue(1.0);
    m_scaleYSpin->setValue(1.0);
    m_scaleZSpin->setValue(1.0);
    onScaleSpinChanged();
}
void VulkanWidget::onResetTranslateX() {
    m_translateXSpin->setValue(0.0);
}
void VulkanWidget::onResetTranslateY() {
    m_translateYSpin->setValue(0.0);
}
void VulkanWidget::onResetTranslateZ() {
    m_translateZSpin->setValue(0.0);
}

void VulkanWidget::onResetRotateX() {
    m_rotateXSpin->setValue(0.0);
}
void VulkanWidget::onResetRotateY() {
    m_rotateYSpin->setValue(0.0);
}
void VulkanWidget::onResetRotateZ() {
    m_rotateZSpin->setValue(0.0);
}

void VulkanWidget::onResetScaleX() {
    m_scaleXSpin->setValue(1.0);
}
void VulkanWidget::onResetScaleY() {
    m_scaleYSpin->setValue(1.0);
}
void VulkanWidget::onResetScaleZ() {
    m_scaleZSpin->setValue(1.0);
}

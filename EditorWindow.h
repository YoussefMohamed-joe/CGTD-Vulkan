#pragma once

#include <QMainWindow>
#include <QMap>
#include <QFocusEvent>
#include <QStyledItemDelegate>
#include <array>
#include "ui_EditorWindow.h"

// Forward declarations
class VulkanWindow;
class QTreeWidgetItem;
class QVulkanInstance;
class QPainter;
class QTimer;
class QPushButton;
class QDoubleSpinBox;

// Include glm for 3D vector types
#include <glm/glm.hpp>


// ===================================================================
// == EyeIconDelegate Declaration
// ===================================================================
class EyeIconDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit EyeIconDelegate(QObject* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;
    QPixmap createFallbackEyeIcon(bool visible) const;
};


// ===================================================================
// == Main Window Declaration
// ===================================================================
class VulkanWidget : public QMainWindow {
    Q_OBJECT

public:
    explicit VulkanWidget(QWidget* parent = nullptr, bool autoInit = true);
    ~VulkanWidget();

    void setupVulkanWindow();

    enum TransformType { Translate, Rotate, Scale };
    Q_ENUM(TransformType)

signals:
    void transformValuesChanged(TransformType type, const glm::vec3& newValues);

public slots:
    void updateTransformPanel(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale);

private slots:
    void onCubeClicked();
    void onSphereClicked();
    void onCylinderClicked();
    void onPyramidClicked();
    void onClearClicked();
    void onScreenshotClicked();
    void onShowAllClicked();
    void onHideAllClicked();
    void onToggleGridClicked();
    void onBackgroundColorClicked();
    void on_outlinerTree_itemChanged(QTreeWidgetItem* item, int column);

    // Slots for properties panel
    void onTranslateSpinChanged();
    void onRotateSpinChanged();
    void onScaleSpinChanged();

    // CORRECTED: Reset functions moved to be slots
    void onResetTranslate();
    void onResetRotate();
    void onResetScale();
    void onResetTranslateX();
    void onResetTranslateY();
    void onResetTranslateZ();

    void onResetRotateX();
    void onResetRotateY();
    void onResetRotateZ();

    void onResetScaleX();
    void onResetScaleY();
    void onResetScaleZ();

private:
    void connectSignals();
    void setupDesign();
    void setupPropertiesPanel();
    void applyBaseStyles();
    void applyLayoutStyles();
    void applyInputStyles();
    void applyNavigationStyles();
    QVulkanInstance* createVulkanInstance();
    bool m_isVulkanInitialized = false;
    void initializeOverlaySystem();
    void setupOverlayWidget();
    void setupOverlayProperties(QWidget* overlay);
    void initializeButtonArray();
    void updateOverlayGeometry();
    void positionButtons(const QSize& renderSize);

    QTimer* m_overlayHeartbeatTimer = nullptr;
    // Pointers for spin boxes
    QDoubleSpinBox* m_translateXSpin = nullptr;
    QDoubleSpinBox* m_translateYSpin = nullptr;
    QDoubleSpinBox* m_translateZSpin = nullptr;
    QDoubleSpinBox* m_rotateXSpin = nullptr;
    QDoubleSpinBox* m_rotateYSpin = nullptr;
    QDoubleSpinBox* m_rotateZSpin = nullptr;
    QDoubleSpinBox* m_scaleXSpin = nullptr;
    QDoubleSpinBox* m_scaleYSpin = nullptr;
    QDoubleSpinBox* m_scaleZSpin = nullptr;

    // Member variables
    Ui::VulkanWidget* ui;
    VulkanWindow* m_vulkanWindow = nullptr;
    QWidget* m_wrapper = nullptr;
    EyeIconDelegate* m_eyeDelegate = nullptr;
    QMap<QTreeWidgetItem*, int> m_primitiveItems;
    static constexpr int BUTTON_COUNT = 4;
    static constexpr int BUTTON_WIDTH = 120;
    static constexpr int BUTTON_HEIGHT = 40;
    static constexpr int SPACING = 10;
    static constexpr int TOP_MARGIN = 10;
    static constexpr int ANIMATION_DURATION = 200;
    std::array<QPushButton*, BUTTON_COUNT> m_overlayButtons;
    bool m_overlayInitialized = false;
    QTimer* m_overlayUpdateTimer = nullptr;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
};
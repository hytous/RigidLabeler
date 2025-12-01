#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "model/TiePointModel.h"
#include "model/ImagePairModel.h"
#include "app/BackendClient.h"
#include "app/AppConfig.h"
#include "PreviewDialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsView>
#include <QGraphicsItemGroup>
#include <QGraphicsLineItem>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QRubberBand>
#include <QUndoStack>
#include <QTextStream>
#include <QScrollBar>
#include <QLineF>
#include <QCoreApplication>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QLabel>
#include <QTimer>

// ============================================================================
// Undo Command Classes
// ============================================================================

// Undo command for removing a tie point (still needed for delete button)
class RemoveTiePointCommand : public QUndoCommand
{
public:
    RemoveTiePointCommand(TiePointModel *model, int index, QUndoCommand *parent = nullptr)
        : QUndoCommand(parent), m_model(model), m_index(index)
    {
        setText(QObject::tr("Delete Tie Point"));
        // Store the tie point data before removal
        TiePoint tp = m_model->getTiePoint(index);
        m_fixed = tp.fixed;
        m_moving = tp.moving;
    }
    
    void redo() override {
        m_model->removeTiePoint(m_index);
    }
    
    void undo() override {
        m_model->insertTiePoint(m_index, m_fixed, m_moving);
    }
    
private:
    TiePointModel *m_model;
    int m_index;
    QPointF m_fixed;
    QPointF m_moving;
};

// ============================================================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_tiePointModel(new TiePointModel(this))
    , m_imagePairModel(new ImagePairModel(this))
    , m_backendClient(nullptr)
    , m_fixedScene(new QGraphicsScene(this))
    , m_movingScene(new QGraphicsScene(this))
    , m_fixedPixmapItem(nullptr)
    , m_movingPixmapItem(nullptr)
    , m_pendingPointMarker(nullptr)
    , m_cursorMarker(nullptr)
    , m_cursorMarkerScene(nullptr)
    , m_hasValidTransform(false)
    , m_isAddingPoint(false)
    , m_zoomFactor(1.0)
    , m_fixedImageIndex(-1)
    , m_movingImageIndex(-1)
    , m_isPanning(false)
    , m_isSelecting(false)
    , m_fixedRubberBand(nullptr)
    , m_movingRubberBand(nullptr)
    , m_undoStack(new QUndoStack(this))
    , m_translator(new QTranslator(this))
    , m_currentLanguage("en")
    , m_realtimeComputeTimer(new QTimer(this))
    , m_realtimeComputeEnabled(false)
    , m_realtimeComputePending(false)
    , m_useTopLeftOrigin(false)  // Default: use image center as origin
    , m_previewDialog(nullptr)
    , m_currentPreviewGridSize(8)
{
    ui->setupUi(this);
    
    // Setup transform mode combo box with tooltips
    ui->cmbTransformMode->setItemData(0, 
        tr("Rigid: Rotation + Translation only\n"
           "Parameters: θ (rotation), tx, ty (translation)\n"
           "Minimum points: 2"), Qt::ToolTipRole);
    ui->cmbTransformMode->setItemData(1, 
        tr("Similarity: Rotation + Translation + Uniform Scale\n"
           "Parameters: θ (rotation), tx, ty (translation), scale\n"
           "Minimum points: 2"), Qt::ToolTipRole);
    ui->cmbTransformMode->setItemData(2, 
        tr("Affine: Full 6-DOF transformation\n"
           "Parameters: θ (rotation), tx, ty (translation), scale_x, scale_y, shear\n"
           "Minimum points: 3"), Qt::ToolTipRole);
    
    // Initialize real-time compute timer (10 seconds)
    m_realtimeComputeTimer->setSingleShot(true);
    m_realtimeComputeTimer->setInterval(10000);  // 10 seconds
    
    // Initialize color palette for point pairs (distinct, easily visible colors)
    m_pointColors << QColor(255, 0, 0)      // Red
                  << QColor(0, 200, 0)      // Green
                  << QColor(0, 100, 255)    // Blue
                  << QColor(255, 165, 0)    // Orange
                  << QColor(148, 0, 211)    // Purple
                  << QColor(0, 206, 209)    // Cyan
                  << QColor(255, 20, 147)   // Pink
                  << QColor(255, 255, 0)    // Yellow
                  << QColor(139, 69, 19)    // Brown
                  << QColor(0, 128, 0)      // Dark Green
                  << QColor(75, 0, 130)     // Indigo
                  << QColor(255, 99, 71);   // Tomato
    
    // Load configuration
    AppConfig::instance().load();
    
    // Create backend client
    m_backendClient = new BackendClient(AppConfig::instance().backendBaseUrl(), this);
    
    // Setup UI components
    setupImageViews();
    installEventFilters();
    setupConnections();
    
    // Setup tie point table model
    ui->tiePointsTable->setModel(m_tiePointModel);
    
    // Setup status bar labels
    m_backendStatusLabel = new QLabel(tr("Backend: Checking..."));
    m_pointCountLabel = new QLabel(tr("Points: 0"));
    m_zoomLabel = new QLabel(tr("Zoom: 100%"));
    statusBar()->addWidget(m_backendStatusLabel);
    statusBar()->addPermanentWidget(m_pointCountLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
    
    // Initial state update
    updateActionStates();
    
    // Check backend health
    m_backendClient->healthCheck();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupImageViews()
{
    // Setup fixed image view
    ui->fixedImageView->setScene(m_fixedScene);
    ui->fixedImageView->setRenderHint(QPainter::Antialiasing);
    ui->fixedImageView->setRenderHint(QPainter::SmoothPixmapTransform);
    ui->fixedImageView->setDragMode(QGraphicsView::NoDrag);
    ui->fixedImageView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    
    // Setup moving image view
    ui->movingImageView->setScene(m_movingScene);
    ui->movingImageView->setRenderHint(QPainter::Antialiasing);
    ui->movingImageView->setRenderHint(QPainter::SmoothPixmapTransform);
    ui->movingImageView->setDragMode(QGraphicsView::NoDrag);
    ui->movingImageView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
}

void MainWindow::installEventFilters()
{
    // Install event filters for mouse wheel zoom and point clicking
    ui->fixedImageView->viewport()->installEventFilter(this);
    ui->movingImageView->viewport()->installEventFilter(this);
    
    // Enable mouse tracking for cursor marker updates
    ui->fixedImageView->viewport()->setMouseTracking(true);
    ui->movingImageView->viewport()->setMouseTracking(true);
}

void MainWindow::setupConnections()
{
    // Menu/Toolbar actions (from .ui file)
    connect(ui->actionLoadFixedImage, &QAction::triggered, this, &MainWindow::loadFixedImage);
    connect(ui->actionLoadMovingImage, &QAction::triggered, this, &MainWindow::loadMovingImage);
    connect(ui->actionSaveLabel, &QAction::triggered, this, &MainWindow::saveLabel);
    connect(ui->actionLoadLabel, &QAction::triggered, this, &MainWindow::loadLabel);
    connect(ui->actionClearPoints, &QAction::triggered, this, &MainWindow::clearAllTiePoints);
    connect(ui->actionZoomIn, &QAction::triggered, this, &MainWindow::zoomIn);
    connect(ui->actionZoomOut, &QAction::triggered, this, &MainWindow::zoomOut);
    connect(ui->actionFitToWindow, &QAction::triggered, this, &MainWindow::zoomToFitAll);
    connect(ui->actionSyncViews, &QAction::toggled, this, &MainWindow::toggleLinkViews);
    connect(ui->actionCompute, &QAction::triggered, this, &MainWindow::computeTransform);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAbout);
    connect(ui->actionUndo, &QAction::triggered, this, &MainWindow::undo);
    connect(ui->actionRedo, &QAction::triggered, this, &MainWindow::redo);
    
    // Language actions
    connect(ui->actionLangEnglish, &QAction::triggered, this, &MainWindow::switchToEnglish);
    connect(ui->actionLangChinese, &QAction::triggered, this, &MainWindow::switchToChinese);
    
    // Panel buttons
    connect(ui->btnLoadFixed, &QPushButton::clicked, this, &MainWindow::loadFixedImage);
    connect(ui->btnLoadMoving, &QPushButton::clicked, this, &MainWindow::loadMovingImage);
    connect(ui->btnPrevFixed, &QPushButton::clicked, this, &MainWindow::prevFixedImage);
    connect(ui->btnNextFixed, &QPushButton::clicked, this, &MainWindow::nextFixedImage);
    connect(ui->btnPrevMoving, &QPushButton::clicked, this, &MainWindow::prevMovingImage);
    connect(ui->btnNextMoving, &QPushButton::clicked, this, &MainWindow::nextMovingImage);
    connect(ui->btnPrevPair, &QPushButton::clicked, this, &MainWindow::prevPair);
    connect(ui->btnNextPair, &QPushButton::clicked, this, &MainWindow::nextPair);
    connect(ui->btnZoomFitFixed, &QPushButton::clicked, this, &MainWindow::zoomToFitFixed);
    connect(ui->btnZoomFitMoving, &QPushButton::clicked, this, &MainWindow::zoomToFitMoving);
    connect(ui->btnAddPoint, &QPushButton::clicked, this, &MainWindow::addTiePoint);
    connect(ui->btnDeletePoint, &QPushButton::clicked, this, &MainWindow::deleteSelectedTiePoint);
    connect(ui->btnClearPoints, &QPushButton::clicked, this, &MainWindow::clearAllTiePoints);
    connect(ui->btnExportPoints, &QPushButton::clicked, this, &MainWindow::exportTiePoints);
    connect(ui->btnImportPoints, &QPushButton::clicked, this, &MainWindow::importTiePoints);
    connect(ui->btnCompute, &QPushButton::clicked, this, &MainWindow::computeTransform);
    connect(ui->btnSaveLabel, &QPushButton::clicked, this, &MainWindow::saveLabel);
    connect(ui->btnLoadLabel, &QPushButton::clicked, this, &MainWindow::loadLabel);
    connect(ui->btnPreview, &QPushButton::clicked, this, &MainWindow::previewWarp);
    connect(ui->btnExportMatrix, &QPushButton::clicked, this, &MainWindow::exportMatrix);
    
    // Options
    connect(ui->chkOriginTopLeft, &QCheckBox::toggled, this, &MainWindow::onOriginModeToggled);
    connect(ui->chkRealtimeCompute, &QCheckBox::toggled, this, &MainWindow::onRealtimeComputeToggled);
    connect(ui->cmbTransformMode, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MainWindow::updateActionStates);
    
    // Real-time compute timer
    connect(m_realtimeComputeTimer, &QTimer::timeout, this, &MainWindow::onRealtimeComputeTimeout);
    
    // Tie point model - pair completed signal for real-time compute
    connect(m_tiePointModel, &TiePointModel::pairCompleted, this, &MainWindow::onPairCompleted);
    
    // Tie point table selection
    connect(ui->tiePointsTable->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onTiePointSelectionChanged);
    
    // Tie point model changes
    connect(m_tiePointModel, &TiePointModel::rowsInserted, this, &MainWindow::updateTiePointViews);
    connect(m_tiePointModel, &TiePointModel::rowsRemoved, this, &MainWindow::updateTiePointViews);
    connect(m_tiePointModel, &TiePointModel::dataChanged, this, &MainWindow::updateTiePointViews);
    connect(m_tiePointModel, &TiePointModel::modelReset, this, &MainWindow::updateTiePointViews);
    
    // Image pair model changes
    connect(m_imagePairModel, &ImagePairModel::fixedImageChanged, this, &MainWindow::updateImageViews);
    connect(m_imagePairModel, &ImagePairModel::movingImageChanged, this, &MainWindow::updateImageViews);
    
    // Backend client responses
    connect(m_backendClient, &BackendClient::healthCheckCompleted, this, &MainWindow::onHealthCheckCompleted);
    connect(m_backendClient, &BackendClient::computeRigidCompleted, this, &MainWindow::onComputeRigidCompleted);
    connect(m_backendClient, &BackendClient::saveLabelCompleted, this, &MainWindow::onSaveLabelCompleted);
    connect(m_backendClient, &BackendClient::loadLabelCompleted, this, &MainWindow::onLoadLabelCompleted);
    connect(m_backendClient, &BackendClient::checkerboardPreviewCompleted, this, &MainWindow::onCheckerboardPreviewCompleted);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    QGraphicsView *view = nullptr;
    QRubberBand **rubberBand = nullptr;
    bool isFixed = false;
    
    if (obj == ui->fixedImageView->viewport()) {
        view = ui->fixedImageView;
        rubberBand = &m_fixedRubberBand;
        isFixed = true;
    } else if (obj == ui->movingImageView->viewport()) {
        view = ui->movingImageView;
        rubberBand = &m_movingRubberBand;
        isFixed = false;
    }
    
    // Handle key press events
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            // Cancel adding point mode
            if (m_isAddingPoint) {
                m_isAddingPoint = false;
                clearCursorMarker();
                ui->fixedImageView->setCursor(Qt::ArrowCursor);
                ui->movingImageView->setCursor(Qt::ArrowCursor);
                statusBar()->showMessage(tr("Point adding cancelled"), 2000);
                return true;
            }
        }
    }
    
    // Handle wheel events for zoom
    if (event->type() == QEvent::Wheel && view) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
        wheelEventOnView(view, wheelEvent);
        if (ui->chkSyncZoom->isChecked()) {
            QGraphicsView *otherView = isFixed ? ui->movingImageView : ui->fixedImageView;
            wheelEventOnView(otherView, wheelEvent);
        }
        return true;
    }
    
    // Handle mouse press
    if (event->type() == QEvent::MouseButtonPress && view) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        
        if (mouseEvent->button() == Qt::LeftButton) {
            // Ctrl+Click: Start panning
            if (mouseEvent->modifiers() & Qt::ControlModifier) {
                m_isPanning = true;
                m_lastPanPos = mouseEvent->pos();
                view->setCursor(Qt::ClosedHandCursor);
                return true;
            }
            // Shift+Click: Start rubber band selection
            else if (mouseEvent->modifiers() & Qt::ShiftModifier) {
                m_isSelecting = true;
                m_rubberBandOrigin = mouseEvent->pos();
                if (!*rubberBand) {
                    *rubberBand = new QRubberBand(QRubberBand::Rectangle, view->viewport());
                }
                (*rubberBand)->setGeometry(QRect(m_rubberBandOrigin, QSize()));
                (*rubberBand)->show();
                return true;
            }
            // Normal click when adding point
            else if (m_isAddingPoint) {
                QPointF scenePos = view->mapToScene(mouseEvent->pos());
                if (isFixed) {
                    onFixedViewClicked(scenePos);
                } else {
                    onMovingViewClicked(scenePos);
                }
                return true;
            }
            // Normal click: select point
            else {
                QPointF scenePos = view->mapToScene(mouseEvent->pos());
                int pointIndex = findPointAtPosition(view, scenePos);
                if (pointIndex >= 0) {
                    ui->tiePointsTable->selectRow(pointIndex);
                }
                return true;
            }
        }
    }
    
    // Handle mouse move
    if (event->type() == QEvent::MouseMove && view) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        
        // Panning with Ctrl+drag
        if (m_isPanning) {
            QPoint delta = mouseEvent->pos() - m_lastPanPos;
            m_lastPanPos = mouseEvent->pos();
            view->horizontalScrollBar()->setValue(view->horizontalScrollBar()->value() - delta.x());
            view->verticalScrollBar()->setValue(view->verticalScrollBar()->value() - delta.y());
            
            // Sync panning if enabled
            if (ui->chkSyncZoom->isChecked()) {
                QGraphicsView *otherView = isFixed ? ui->movingImageView : ui->fixedImageView;
                otherView->horizontalScrollBar()->setValue(otherView->horizontalScrollBar()->value() - delta.x());
                otherView->verticalScrollBar()->setValue(otherView->verticalScrollBar()->value() - delta.y());
            }
            return true;
        }
        // Rubber band selection with Shift+drag
        else if (m_isSelecting && *rubberBand) {
            (*rubberBand)->setGeometry(QRect(m_rubberBandOrigin, mouseEvent->pos()).normalized());
            return true;
        }
        // Update cursor marker when in adding point mode
        else if (m_isAddingPoint) {
            QPointF scenePos = view->mapToScene(mouseEvent->pos());
            QGraphicsScene *targetScene = isFixed ? m_fixedScene : m_movingScene;
            
            // Show cursor marker on the view the mouse is over
            updateCursorMarker(targetScene, scenePos);
        }
    }
    
    // Handle mouse leave - hide cursor marker
    if (event->type() == QEvent::Leave && view) {
        if (m_isAddingPoint) {
            clearCursorMarker();
        }
    }
    
    // Handle mouse release
    if (event->type() == QEvent::MouseButtonRelease && view) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        
        if (mouseEvent->button() == Qt::LeftButton) {
            // End panning
            if (m_isPanning) {
                m_isPanning = false;
                // Restore cursor based on current mode
                if (m_isAddingPoint) {
                    view->setCursor(Qt::CrossCursor);
                } else {
                    view->setCursor(Qt::ArrowCursor);
                }
                return true;
            }
            // End rubber band selection
            else if (m_isSelecting && *rubberBand) {
                m_isSelecting = false;
                QRect rubberBandRect = (*rubberBand)->geometry();
                (*rubberBand)->hide();
                
                if (rubberBandRect.width() > 5 && rubberBandRect.height() > 5) {
                    handleRubberBandSelection(view, rubberBandRect);
                }
                return true;
            }
        }
    }
    
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::wheelEventOnView(QGraphicsView *view, QWheelEvent *event)
{
    const double scaleFactor = 1.15;
    if (event->angleDelta().y() > 0) {
        view->scale(scaleFactor, scaleFactor);
        m_zoomFactor *= scaleFactor;
    } else {
        view->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        m_zoomFactor /= scaleFactor;
    }
    m_zoomLabel->setText(tr("Zoom: %1%").arg(int(m_zoomFactor * 100)));
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        // Cancel adding point mode
        if (m_isAddingPoint) {
            m_isAddingPoint = false;
            clearCursorMarker();
            ui->fixedImageView->setCursor(Qt::ArrowCursor);
            ui->movingImageView->setCursor(Qt::ArrowCursor);
            statusBar()->showMessage(tr("Point adding cancelled"), 2000);
            event->accept();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

// ============================================================================
// File Operations
// ============================================================================

void MainWindow::loadFixedImage()
{
    QString dir = AppConfig::instance().lastFixedImageDir();
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Load Fixed Image"),
        dir,
        tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)")
    );
    
    if (fileName.isEmpty())
        return;
    
    if (m_imagePairModel->loadFixedImage(fileName)) {
        QFileInfo fi(fileName);
        AppConfig::instance().setLastFixedImageDir(fi.absolutePath());
        m_fixedImageDir = fi.absolutePath();
        m_fixedImageFiles = getImageFilesInDir(m_fixedImageDir);
        m_fixedImageIndex = m_fixedImageFiles.indexOf(fi.fileName());
        // Update filename label
        ui->lblFixedFileName->setText(tr("%1 (%2/%3)")
            .arg(fi.fileName())
            .arg(m_fixedImageIndex + 1)
            .arg(m_fixedImageFiles.size()));
        statusBar()->showMessage(tr("Fixed image loaded: %1").arg(fileName), 3000);
    } else {
        showError(tr("Error"), tr("Failed to load fixed image."));
    }
    
    updateActionStates();
}

void MainWindow::loadMovingImage()
{
    QString dir = AppConfig::instance().lastMovingImageDir();
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Load Moving Image"),
        dir,
        tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)")
    );
    
    if (fileName.isEmpty())
        return;
    
    if (m_imagePairModel->loadMovingImage(fileName)) {
        QFileInfo fi(fileName);
        AppConfig::instance().setLastMovingImageDir(fi.absolutePath());
        m_movingImageDir = fi.absolutePath();
        m_movingImageFiles = getImageFilesInDir(m_movingImageDir);
        m_movingImageIndex = m_movingImageFiles.indexOf(fi.fileName());
        // Update filename label
        ui->lblMovingFileName->setText(tr("%1 (%2/%3)")
            .arg(fi.fileName())
            .arg(m_movingImageIndex + 1)
            .arg(m_movingImageFiles.size()));
        statusBar()->showMessage(tr("Moving image loaded: %1").arg(fileName), 3000);
    } else {
        showError(tr("Error"), tr("Failed to load moving image."));
    }
    
    updateActionStates();
}

void MainWindow::clearImages()
{
    m_imagePairModel->clearImages();
    m_tiePointModel->clearAll();
    clearPendingPointMarker();
    clearCursorMarker();
    m_fixedScene->clear();
    m_movingScene->clear();
    m_fixedPixmapItem = nullptr;
    m_movingPixmapItem = nullptr;
    m_fixedPointMarkers.clear();
    m_movingPointMarkers.clear();
    ui->txtResult->clear();
    m_hasValidTransform = false;
    m_isAddingPoint = false;
    updateActionStates();
}

// ============================================================================
// Label Operations
// ============================================================================

void MainWindow::saveLabel()
{
    if (!m_imagePairModel->hasBothImages()) {
        showError(tr("Error"), tr("Please load both fixed and moving images first."));
        return;
    }
    
    if (!m_hasValidTransform) {
        showError(tr("Error"), tr("Please compute transform first."));
        return;
    }
    
    // Collect tie points
    QList<QPair<QPointF, QPointF>> tiePoints;
    for (const TiePoint &tp : m_tiePointModel->getAllTiePoints()) {
        tiePoints.append({tp.fixed, tp.moving});
    }
    
    RigidParams rigid;
    rigid.theta_deg = m_currentTheta;
    rigid.tx = m_currentTx;
    rigid.ty = m_currentTy;
    rigid.scale_x = m_currentScaleX;
    rigid.scale_y = m_currentScaleY;
    rigid.shear = m_currentShear;
    
    m_backendClient->saveLabel(
        m_imagePairModel->fixedImagePath(),
        m_imagePairModel->movingImagePath(),
        rigid,
        m_currentMatrix,
        tiePoints
    );
    
    statusBar()->showMessage(tr("Saving label..."), 2000);
}

void MainWindow::loadLabel()
{
    if (!m_imagePairModel->hasBothImages()) {
        showError(tr("Error"), tr("Please load both fixed and moving images first."));
        return;
    }
    
    m_backendClient->loadLabel(
        m_imagePairModel->fixedImagePath(),
        m_imagePairModel->movingImagePath()
    );
    
    statusBar()->showMessage(tr("Loading label..."), 2000);
}

// ============================================================================
// Transform Operations
// ============================================================================

void MainWindow::computeTransform()
{
    int minPoints = AppConfig::instance().minPointsRequired();
    int completeCount = m_tiePointModel->completePairCount();
    
    if (completeCount < minPoints) {
        showError(tr("Error"), 
            tr("Need at least %1 complete tie points to compute transform. Currently have %2.")
                .arg(minPoints).arg(completeCount));
        return;
    }
    
    // Get image centers for coordinate conversion
    double fixedCenterX = 0, fixedCenterY = 0;
    double movingCenterX = 0, movingCenterY = 0;
    
    if (!m_useTopLeftOrigin) {
        if (m_fixedPixmapItem) {
            QPixmap pm = m_fixedPixmapItem->pixmap();
            fixedCenterX = pm.width() / 2.0;
            fixedCenterY = pm.height() / 2.0;
        }
        if (m_movingPixmapItem) {
            QPixmap pm = m_movingPixmapItem->pixmap();
            movingCenterX = pm.width() / 2.0;
            movingCenterY = pm.height() / 2.0;
        }
    }
    
    // Collect complete tie points only, converting to appropriate coordinate system
    QList<QPair<QPointF, QPointF>> tiePoints;
    for (const TiePoint &tp : m_tiePointModel->getAllTiePoints()) {
        QPointF fixed = tp.fixed;
        QPointF moving = tp.moving;
        
        // Convert to center-origin coordinates if needed
        if (!m_useTopLeftOrigin) {
            fixed.setX(fixed.x() - fixedCenterX);
            fixed.setY(fixed.y() - fixedCenterY);
            moving.setX(moving.x() - movingCenterX);
            moving.setY(moving.y() - movingCenterY);
        }
        
        tiePoints.append({fixed, moving});
    }
    
    // Get transform mode from combo box
    QString transformMode;
    int modeIndex = ui->cmbTransformMode->currentIndex();
    switch (modeIndex) {
        case 0: transformMode = "rigid"; break;
        case 1: transformMode = "similarity"; break;
        case 2: 
        default: transformMode = "affine"; break;
    }
    
    m_backendClient->computeRigid(
        tiePoints,
        transformMode,
        minPoints
    );
    
    statusBar()->showMessage(tr("Computing transform..."), 2000);
}

void MainWindow::previewWarp()
{
    if (!m_hasValidTransform) {
        showError(tr("Error"), tr("Please compute transform first."));
        return;
    }
    
    // Check if images are loaded
    if (m_imagePairModel->fixedImagePath().isEmpty() || 
        m_imagePairModel->movingImagePath().isEmpty()) {
        showError(tr("Error"), tr("Please load both fixed and moving images first."));
        return;
    }
    
    // Create preview dialog if not exists
    if (!m_previewDialog) {
        m_previewDialog = new PreviewDialog(this);
        connect(m_previewDialog, &PreviewDialog::refreshRequested, 
                this, &MainWindow::onPreviewRefreshRequested);
    }
    
    // Set initial grid size
    m_previewDialog->setGridSize(m_currentPreviewGridSize);
    
    // Show loading state
    m_previewDialog->showLoading();
    m_previewDialog->show();
    m_previewDialog->raise();
    m_previewDialog->activateWindow();
    
    // Request checkerboard preview from backend
    // Note: use_center_origin should be true when m_useTopLeftOrigin is false
    // because the matrix was computed with center origin when m_useTopLeftOrigin is false
    m_backendClient->requestCheckerboardPreview(
        m_imagePairModel->fixedImagePath(),
        m_imagePairModel->movingImagePath(),
        m_currentMatrix,
        m_currentPreviewGridSize,
        !m_useTopLeftOrigin  // use_center_origin = !m_useTopLeftOrigin
    );
    
    statusBar()->showMessage(tr("Generating checkerboard preview..."), 3000);
}

// ============================================================================
// View Operations
// ============================================================================

void MainWindow::zoomIn()
{
    const double scaleFactor = 1.25;
    ui->fixedImageView->scale(scaleFactor, scaleFactor);
    if (ui->chkSyncZoom->isChecked()) {
        ui->movingImageView->scale(scaleFactor, scaleFactor);
    }
    m_zoomFactor *= scaleFactor;
    m_zoomLabel->setText(tr("Zoom: %1%").arg(int(m_zoomFactor * 100)));
}

void MainWindow::zoomOut()
{
    const double scaleFactor = 1.25;
    ui->fixedImageView->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    if (ui->chkSyncZoom->isChecked()) {
        ui->movingImageView->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    }
    m_zoomFactor /= scaleFactor;
    m_zoomLabel->setText(tr("Zoom: %1%").arg(int(m_zoomFactor * 100)));
}

void MainWindow::zoomToFitFixed()
{
    if (m_fixedPixmapItem) {
        ui->fixedImageView->fitInView(m_fixedPixmapItem, Qt::KeepAspectRatio);
    }
}

void MainWindow::zoomToFitMoving()
{
    if (m_movingPixmapItem) {
        ui->movingImageView->fitInView(m_movingPixmapItem, Qt::KeepAspectRatio);
    }
}

void MainWindow::zoomToFitAll()
{
    zoomToFitFixed();
    zoomToFitMoving();
}

void MainWindow::toggleLinkViews(bool linked)
{
    ui->chkSyncZoom->setChecked(linked);
}

// ============================================================================
// Tie Point Operations
// ============================================================================

void MainWindow::addTiePoint()
{
    m_isAddingPoint = true;
    ui->fixedImageView->setCursor(Qt::CrossCursor);
    ui->movingImageView->setCursor(Qt::CrossCursor);
    statusBar()->showMessage(tr("Click on either image to add a point. Press Escape to cancel."), 5000);
}

void MainWindow::deleteSelectedTiePoint()
{
    QModelIndexList selected = ui->tiePointsTable->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        statusBar()->showMessage(tr("No tie point selected."), 2000);
        return;
    }
    
    // Remove in reverse order to keep indices valid (use undo commands)
    QList<int> rows;
    for (const QModelIndex &index : selected) {
        rows.append(index.row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    
    m_undoStack->beginMacro(tr("Delete %1 tie point(s)").arg(rows.size()));
    for (int row : rows) {
        m_undoStack->push(new RemoveTiePointCommand(m_tiePointModel, row));
    }
    m_undoStack->endMacro();
    
    m_hasValidTransform = false;
    updateActionStates();
    statusBar()->showMessage(tr("Tie point(s) deleted."), 2000);
}

void MainWindow::clearAllTiePoints()
{
    int totalCount = m_tiePointModel->pairCount();
    if (totalCount == 0)
        return;
        
    int ret = QMessageBox::question(this, tr("Clear All"),
        tr("Are you sure you want to clear all %1 tie point(s)?").arg(totalCount),
        QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        m_undoStack->clear();  // Clear undo history since this is a reset
        m_tiePointModel->clearAll();
        m_hasValidTransform = false;
        ui->txtResult->clear();
        updatePointDisplay();
        updateActionStates();
        statusBar()->showMessage(tr("All tie points cleared."), 2000);
    }
}

void MainWindow::onTiePointSelectionChanged()
{
    // Redraw all points with proper highlighting
    updatePointDisplay();
}

void MainWindow::onFixedViewClicked(const QPointF &pos)
{
    // Clear cursor marker first (it was following the mouse)
    clearCursorMarker();
    
    // Add fixed point to model (model handles internal undo stack)
    int index = m_tiePointModel->addFixedPoint(pos);
    
    // Display coordinates in current coordinate system
    ui->lblFixedCoord->setText(formatDisplayCoord(pos, true));
    QPointF displayPos = pixelToDisplayCoord(pos, true);
    
    // Update display
    updatePointDisplay();
    updateActionStates();
    
    // Check if this completes a pair
    if (m_tiePointModel->hasBothPoints(index)) {
        statusBar()->showMessage(tr("Fixed point added at (%1, %2). Pair #%3 complete!")
            .arg(displayPos.x(), 0, 'f', 1).arg(displayPos.y(), 0, 'f', 1).arg(index + 1), 3000);
        m_hasValidTransform = false;
    } else {
        statusBar()->showMessage(tr("Fixed point added at (%1, %2). Now add moving point for pair #%3.")
            .arg(displayPos.x(), 0, 'f', 1).arg(displayPos.y(), 0, 'f', 1).arg(index + 1), 5000);
    }
}

void MainWindow::onMovingViewClicked(const QPointF &pos)
{
    // Clear cursor marker first
    clearCursorMarker();
    
    // Add moving point to model (model handles internal undo stack)
    int index = m_tiePointModel->addMovingPoint(pos);
    
    // Display coordinates in current coordinate system
    ui->lblMovingCoord->setText(formatDisplayCoord(pos, false));
    QPointF displayPos = pixelToDisplayCoord(pos, false);
    
    // Update display
    updatePointDisplay();
    updateActionStates();
    
    // Check if this completes a pair
    if (m_tiePointModel->hasBothPoints(index)) {
        statusBar()->showMessage(tr("Moving point added at (%1, %2). Pair #%3 complete!")
            .arg(displayPos.x(), 0, 'f', 1).arg(displayPos.y(), 0, 'f', 1).arg(index + 1), 3000);
        m_hasValidTransform = false;
    } else {
        statusBar()->showMessage(tr("Moving point added at (%1, %2). Now add fixed point for pair #%3.")
            .arg(displayPos.x(), 0, 'f', 1).arg(displayPos.y(), 0, 'f', 1).arg(index + 1), 5000);
    }
}

// ============================================================================
// ============================================================================
// Backend Responses
// ============================================================================

void MainWindow::onHealthCheckCompleted(const HealthCheckResult &result)
{
    if (result.success) {
        m_backendStatusLabel->setText(tr("Backend: Online (v%1)").arg(result.version));
        m_backendStatusLabel->setStyleSheet("color: green;");
    } else {
        m_backendStatusLabel->setText(tr("Backend: Offline"));
        m_backendStatusLabel->setStyleSheet("color: red;");
    }
}

void MainWindow::onComputeRigidCompleted(const ComputeRigidResult &result)
{
    if (!result.success) {
        showError(tr("Compute Error"), result.errorMessage);
        return;
    }
    
    // Store results
    m_hasValidTransform = true;
    m_currentTheta = result.rigid.theta_deg;
    m_currentTx = result.rigid.tx;
    m_currentTy = result.rigid.ty;
    m_currentScaleX = result.rigid.scale_x;
    m_currentScaleY = result.rigid.scale_y;
    m_currentShear = result.rigid.shear;
    m_currentMatrix = result.matrix3x3;
    
    // Display results
    QString resultText;
    resultText += tr("Rotation: %1°\n").arg(result.rigid.theta_deg, 0, 'f', 4);
    resultText += tr("Translation: (%1, %2)\n").arg(result.rigid.tx, 0, 'f', 4).arg(result.rigid.ty, 0, 'f', 4);
    resultText += tr("Scale X: %1\n").arg(result.rigid.scale_x, 0, 'f', 6);
    resultText += tr("Scale Y: %1\n").arg(result.rigid.scale_y, 0, 'f', 6);
    if (qAbs(result.rigid.shear) > 1e-6) {
        resultText += tr("Shear: %1\n").arg(result.rigid.shear, 0, 'f', 6);
    }
    resultText += tr("RMS Error: %1 px\n").arg(result.rmsError, 0, 'f', 4);
    resultText += tr("Points Used: %1\n\n").arg(result.numPoints);
    resultText += tr("Matrix:\n");
    for (int i = 0; i < 3; ++i) {
        resultText += QString("  [%1, %2, %3]\n")
            .arg(result.matrix3x3[i][0], 10, 'f', 6)
            .arg(result.matrix3x3[i][1], 10, 'f', 6)
            .arg(result.matrix3x3[i][2], 10, 'f', 6);
    }
    
    ui->txtResult->setText(resultText);
    statusBar()->showMessage(tr("Transform computed successfully."), 3000);
    updateActionStates();
}

void MainWindow::onSaveLabelCompleted(const LabelSaveResult &result)
{
    if (!result.success) {
        showError(tr("Save Error"), result.errorMessage);
        return;
    }
    
    showInfo(tr("Label Saved"), 
        tr("Label saved successfully.\nID: %1\nPath: %2").arg(result.labelId).arg(result.labelPath));
}

void MainWindow::onLoadLabelCompleted(const LabelData &result)
{
    if (!result.success) {
        if (result.errorCode == "LABEL_NOT_FOUND") {
            showInfo(tr("No Label"), tr("No label found for this image pair."));
        } else {
            showError(tr("Load Error"), result.errorMessage);
        }
        return;
    }
    
    // Clear and restore tie points
    m_tiePointModel->clearAll();
    for (const auto &pair : result.tiePoints) {
        m_tiePointModel->addTiePoint(pair.first, pair.second);
    }
    
    // Store transform results
    m_hasValidTransform = true;
    m_currentTheta = result.rigid.theta_deg;
    m_currentTx = result.rigid.tx;
    m_currentTy = result.rigid.ty;
    m_currentScaleX = result.rigid.scale_x;
    m_currentScaleY = result.rigid.scale_y;
    m_currentShear = result.rigid.shear;
    m_currentMatrix = result.matrix3x3;
    
    // Display results
    QString resultText;
    resultText += tr("Rotation: %1°\n").arg(result.rigid.theta_deg, 0, 'f', 4);
    resultText += tr("Translation: (%1, %2)\n").arg(result.rigid.tx, 0, 'f', 4).arg(result.rigid.ty, 0, 'f', 4);
    resultText += tr("Scale X: %1\n").arg(result.rigid.scale_x, 0, 'f', 6);
    resultText += tr("Scale Y: %1\n").arg(result.rigid.scale_y, 0, 'f', 6);
    if (qAbs(result.rigid.shear) > 1e-6) {
        resultText += tr("Shear: %1\n").arg(result.rigid.shear, 0, 'f', 6);
    }
    resultText += tr("\n(Loaded from saved label)");
    
    ui->txtResult->setText(resultText);
    statusBar()->showMessage(tr("Label loaded successfully."), 3000);
    updateActionStates();
}

void MainWindow::onCheckerboardPreviewCompleted(const CheckerboardPreviewResult &result)
{
    if (!m_previewDialog) {
        return;
    }
    
    if (!result.success) {
        m_previewDialog->showError(result.errorMessage);
        statusBar()->showMessage(tr("Preview generation failed: %1").arg(result.errorMessage), 5000);
        return;
    }
    
    // Update preview dialog with the image
    if (m_previewDialog->setImageFromBase64(result.imageBase64, result.width, result.height)) {
        statusBar()->showMessage(tr("Checkerboard preview generated."), 3000);
    } else {
        m_previewDialog->showError(tr("Failed to decode preview image"));
    }
}

void MainWindow::onPreviewRefreshRequested(int gridSize)
{
    if (!m_hasValidTransform) {
        if (m_previewDialog) {
            m_previewDialog->showError(tr("No valid transform available"));
        }
        return;
    }
    
    m_currentPreviewGridSize = gridSize;
    
    // Show loading state
    if (m_previewDialog) {
        m_previewDialog->showLoading();
    }
    
    // Request new preview with updated grid size
    m_backendClient->requestCheckerboardPreview(
        m_imagePairModel->fixedImagePath(),
        m_imagePairModel->movingImagePath(),
        m_currentMatrix,
        gridSize,
        !m_useTopLeftOrigin  // use_center_origin = !m_useTopLeftOrigin
    );
    
    statusBar()->showMessage(tr("Refreshing preview with grid size %1...").arg(gridSize), 2000);
}

// ============================================================================
// About Dialog
// ============================================================================

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About RigidLabeler"),
        tr("<h2>RigidLabeler</h2>"
           "<p>A 2D geometric transformation labeling tool.</p>"
           "<p>Version 0.1.1</p>"
           "<p>This tool allows you to:</p>"
           "<ul>"
           "<li>Load image pairs (fixed and moving)</li>"
           "<li>Define tie points between images</li>"
           "<li>Compute rigid/similarity/affine transforms</li>"
           "<li>Save and load transformation labels</li>"
           "</ul>"));
}

// ============================================================================
// Helper Methods
// ============================================================================

void MainWindow::updateImageViews()
{
    // Update fixed image
    if (m_imagePairModel->hasFixedImage()) {
        if (m_fixedPixmapItem) {
            m_fixedScene->removeItem(m_fixedPixmapItem);
            delete m_fixedPixmapItem;
        }
        m_fixedPixmapItem = m_fixedScene->addPixmap(QPixmap::fromImage(m_imagePairModel->fixedImage()));
        ui->fixedImageView->fitInView(m_fixedPixmapItem, Qt::KeepAspectRatio);
    }
    
    // Update moving image
    if (m_imagePairModel->hasMovingImage()) {
        if (m_movingPixmapItem) {
            m_movingScene->removeItem(m_movingPixmapItem);
            delete m_movingPixmapItem;
        }
        m_movingPixmapItem = m_movingScene->addPixmap(QPixmap::fromImage(m_imagePairModel->movingImage()));
        ui->movingImageView->fitInView(m_movingPixmapItem, Qt::KeepAspectRatio);
    }
    
    // Update coordinate offsets for TiePointModel display
    updateTiePointModelCoordinateOffsets();
    
    updateActionStates();
}

void MainWindow::updateTiePointViews()
{
    updatePointDisplay();
    m_pointCountLabel->setText(tr("Points: %1").arg(m_tiePointModel->count()));
    ui->lblPointCount->setText(tr("Points: %1 (min 2 required)").arg(m_tiePointModel->count()));
    updateActionStates();
}

void MainWindow::updatePointDisplay()
{
    // Clear existing point markers
    for (QGraphicsItemGroup *item : m_fixedPointMarkers) {
        m_fixedScene->removeItem(item);
        delete item;
    }
    m_fixedPointMarkers.clear();
    
    for (QGraphicsItemGroup *item : m_movingPointMarkers) {
        m_movingScene->removeItem(item);
        delete item;
    }
    m_movingPointMarkers.clear();
    
    // Get current selection
    QModelIndexList selected = ui->tiePointsTable->selectionModel()->selectedRows();
    QSet<int> selectedRows;
    for (const QModelIndex &index : selected) {
        selectedRows.insert(index.row());
    }
    
    // Add new point markers with colors - use new getAllPairs() API
    QList<TiePointPair> pairs = m_tiePointModel->getAllPairs();
    for (int i = 0; i < pairs.size(); ++i) {
        const TiePointPair &pair = pairs[i];
        QColor color = m_pointColors[i % m_pointColors.size()];
        bool isSelected = selectedRows.contains(i);
        
        // Only draw fixed marker if fixed point exists
        if (pair.hasFixed()) {
            QGraphicsItemGroup *fixedMarker = createCrosshairMarker(m_fixedScene, pair.fixed.value(), color, isSelected);
            m_fixedPointMarkers.append(fixedMarker);
        }
        
        // Only draw moving marker if moving point exists
        if (pair.hasMoving()) {
            QGraphicsItemGroup *movingMarker = createCrosshairMarker(m_movingScene, pair.moving.value(), color, isSelected);
            m_movingPointMarkers.append(movingMarker);
        }
    }
    
    // Update point count display
    int completeCount = m_tiePointModel->completePairCount();
    int totalCount = m_tiePointModel->pairCount();
    if (completeCount != totalCount) {
        ui->lblPointCount->setText(tr("Points: %1 complete (%2 partial)").arg(completeCount).arg(totalCount - completeCount));
        m_pointCountLabel->setText(tr("Points: %1/%2").arg(completeCount).arg(totalCount));
    } else {
        ui->lblPointCount->setText(tr("Points: %1 (min 2 required)").arg(completeCount));
        m_pointCountLabel->setText(tr("Points: %1").arg(completeCount));
    }
}

QGraphicsItemGroup* MainWindow::createCrosshairMarker(QGraphicsScene *scene, const QPointF &pos, 
                                                       const QColor &color, bool highlighted)
{
    const double armLength = highlighted ? 14.0 : 10.0;  // Larger when highlighted
    const double penWidth = highlighted ? 3.0 : 2.0;     // Thicker when highlighted
    const double gapRadius = 3.0;  // Gap in center
    
    QGraphicsItemGroup *group = new QGraphicsItemGroup();
    
    // Make the group ignore view transformations (zoom/scale)
    // This keeps the marker at a fixed screen size
    group->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    
    QPen pen(color, penWidth);
    
    // Draw lines relative to (0,0) since group will be positioned at 'pos'
    // Horizontal line (left part)
    QGraphicsLineItem *hLineLeft = new QGraphicsLineItem(-armLength, 0, -gapRadius, 0);
    hLineLeft->setPen(pen);
    group->addToGroup(hLineLeft);
    
    // Horizontal line (right part)
    QGraphicsLineItem *hLineRight = new QGraphicsLineItem(gapRadius, 0, armLength, 0);
    hLineRight->setPen(pen);
    group->addToGroup(hLineRight);
    
    // Vertical line (top part)
    QGraphicsLineItem *vLineTop = new QGraphicsLineItem(0, -armLength, 0, -gapRadius);
    vLineTop->setPen(pen);
    group->addToGroup(vLineTop);
    
    // Vertical line (bottom part)
    QGraphicsLineItem *vLineBottom = new QGraphicsLineItem(0, gapRadius, 0, armLength);
    vLineBottom->setPen(pen);
    group->addToGroup(vLineBottom);
    
    // Add a small center dot
    QGraphicsEllipseItem *centerDot = new QGraphicsEllipseItem(-2, -2, 4, 4);
    centerDot->setPen(Qt::NoPen);
    centerDot->setBrush(color);
    group->addToGroup(centerDot);
    
    // Position the group at the scene coordinates
    group->setPos(pos);
    
    scene->addItem(group);
    return group;
}

void MainWindow::updatePendingPointMarker()
{
    // No longer needed - partial points are now shown directly in updatePointDisplay()
    clearPendingPointMarker();
}

void MainWindow::clearPendingPointMarker()
{
    if (m_pendingPointMarker) {
        m_fixedScene->removeItem(m_pendingPointMarker);
        delete m_pendingPointMarker;
        m_pendingPointMarker = nullptr;
    }
}

QColor MainWindow::getNextPointColor() const
{
    int nextIndex = m_tiePointModel->count();
    return m_pointColors[nextIndex % m_pointColors.size()];
}

void MainWindow::updateCursorMarker(QGraphicsScene *scene, const QPointF &pos)
{
    // If cursor marker exists in a different scene, remove it first
    if (m_cursorMarker && m_cursorMarkerScene != scene) {
        clearCursorMarker();
    }
    
    QColor color = getNextPointColor();
    
    if (!m_cursorMarker) {
        // Create new cursor marker
        m_cursorMarker = createCrosshairMarker(scene, pos, color, false);
        m_cursorMarkerScene = scene;
        // Make cursor marker semi-transparent to distinguish from placed points
        m_cursorMarker->setOpacity(0.7);
    } else {
        // Update position directly using setPos (since we use ItemIgnoresTransformations)
        m_cursorMarker->setPos(pos);
    }
}

void MainWindow::clearCursorMarker()
{
    if (m_cursorMarker) {
        if (m_cursorMarkerScene) {
            m_cursorMarkerScene->removeItem(m_cursorMarker);
        }
        delete m_cursorMarker;
        m_cursorMarker = nullptr;
        m_cursorMarkerScene = nullptr;
    }
}

void MainWindow::updateActionStates()
{
    bool hasBothImages = m_imagePairModel->hasBothImages();
    int pointCount = m_tiePointModel->completePairCount();  // Use complete pairs for compute
    int totalCount = m_tiePointModel->pairCount();
    
    // Determine minimum points based on transform mode
    int modeIndex = ui->cmbTransformMode->currentIndex();
    int minPoints = (modeIndex == 2) ? 3 : 2;  // Affine needs 3, others need 2
    minPoints = qMax(minPoints, AppConfig::instance().minPointsRequired());
    
    // Update actions from .ui file
    ui->actionCompute->setEnabled(hasBothImages && pointCount >= minPoints);
    ui->actionSaveLabel->setEnabled(hasBothImages && m_hasValidTransform);
    ui->actionLoadLabel->setEnabled(hasBothImages);
    
    // Undo is enabled if either model or QUndoStack can undo
    ui->actionUndo->setEnabled(m_tiePointModel->canUndo() || m_undoStack->canUndo());
    ui->actionRedo->setEnabled(m_tiePointModel->canRedo() || m_undoStack->canRedo());
    
    // Update buttons
    ui->btnCompute->setEnabled(hasBothImages && pointCount >= minPoints);
    ui->btnSaveLabel->setEnabled(hasBothImages && m_hasValidTransform);
    ui->btnLoadLabel->setEnabled(hasBothImages);
    ui->btnPreview->setEnabled(m_hasValidTransform);
    ui->btnAddPoint->setEnabled(hasBothImages);
    ui->btnDeletePoint->setEnabled(totalCount > 0);
    ui->btnClearPoints->setEnabled(totalCount > 0);
    ui->btnExportPoints->setEnabled(m_tiePointModel->completePairCount() > 0);
    ui->btnImportPoints->setEnabled(hasBothImages);
    
    // Navigation buttons
    ui->btnPrevFixed->setEnabled(m_fixedImageIndex > 0);
    ui->btnNextFixed->setEnabled(m_fixedImageIndex >= 0 && m_fixedImageIndex < m_fixedImageFiles.size() - 1);
    ui->btnPrevMoving->setEnabled(m_movingImageIndex > 0);
    ui->btnNextMoving->setEnabled(m_movingImageIndex >= 0 && m_movingImageIndex < m_movingImageFiles.size() - 1);
    ui->btnPrevPair->setEnabled(ui->btnPrevFixed->isEnabled() || ui->btnPrevMoving->isEnabled());
    ui->btnNextPair->setEnabled(ui->btnNextFixed->isEnabled() || ui->btnNextMoving->isEnabled());
    
    // Matrix export button
    ui->btnExportMatrix->setEnabled(m_hasValidTransform);
    
    // Real-time compute mode - enable checkbox only when 3+ complete pairs
    updateRealtimeComputeState();
}

// ============================================================================
// Coordinate Conversion Helpers
// ============================================================================

QPointF MainWindow::pixelToDisplayCoord(const QPointF &pixelPos, bool isFixed) const
{
    if (m_useTopLeftOrigin) {
        return pixelPos;
    }
    
    // Convert to center-origin coordinates
    double centerX = 0, centerY = 0;
    if (isFixed && m_fixedPixmapItem) {
        QPixmap pm = m_fixedPixmapItem->pixmap();
        centerX = pm.width() / 2.0;
        centerY = pm.height() / 2.0;
    } else if (!isFixed && m_movingPixmapItem) {
        QPixmap pm = m_movingPixmapItem->pixmap();
        centerX = pm.width() / 2.0;
        centerY = pm.height() / 2.0;
    }
    
    return QPointF(pixelPos.x() - centerX, pixelPos.y() - centerY);
}

QString MainWindow::formatDisplayCoord(const QPointF &pixelPos, bool isFixed) const
{
    QPointF displayPos = pixelToDisplayCoord(pixelPos, isFixed);
    return tr("X: %1, Y: %2").arg(displayPos.x(), 0, 'f', 1).arg(displayPos.y(), 0, 'f', 1);
}

void MainWindow::showError(const QString &title, const QString &message)
{
    QMessageBox::critical(this, title, message);
}

void MainWindow::showInfo(const QString &title, const QString &message)
{
    QMessageBox::information(this, title, message);
}

void MainWindow::showSuccessToast(const QString &message, int durationMs)
{
    // Create a frameless, translucent message box
    QLabel *toast = new QLabel(this);
    toast->setText("✓ " + message);
    toast->setStyleSheet(
        "QLabel {"
        "  background-color: rgba(76, 175, 80, 220);"
        "  color: white;"
        "  padding: 12px 24px;"
        "  border-radius: 8px;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "}"
    );
    toast->setAlignment(Qt::AlignCenter);
    toast->adjustSize();
    
    // Position at center-top of the window
    int x = (width() - toast->width()) / 2;
    int y = 80;
    toast->move(x, y);
    toast->show();
    
    // Fade out animation
    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(toast);
    toast->setGraphicsEffect(effect);
    
    QPropertyAnimation *animation = new QPropertyAnimation(effect, "opacity");
    animation->setDuration(500);
    animation->setStartValue(1.0);
    animation->setEndValue(0.0);
    animation->setEasingCurve(QEasingCurve::InQuad);
    
    // Start fade after delay
    QTimer::singleShot(durationMs - 500, [animation]() {
        animation->start();
    });
    
    // Delete toast after animation
    connect(animation, &QPropertyAnimation::finished, toast, &QLabel::deleteLater);
}

// ============================================================================
// Image Navigation
// ============================================================================

QStringList MainWindow::getImageFilesInDir(const QString &dir)
{
    QDir directory(dir);
    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.tif" << "*.tiff";
    QStringList files = directory.entryList(filters, QDir::Files, QDir::Name);
    return files;
}

void MainWindow::loadFixedImageByIndex(int index)
{
    if (index < 0 || index >= m_fixedImageFiles.size())
        return;
    
    QString fileName = m_fixedImageDir + "/" + m_fixedImageFiles[index];
    if (m_imagePairModel->loadFixedImage(fileName)) {
        m_fixedImageIndex = index;
        // Update filename label
        ui->lblFixedFileName->setText(tr("%1 (%2/%3)")
            .arg(m_fixedImageFiles[index])
            .arg(index + 1)
            .arg(m_fixedImageFiles.size()));
        statusBar()->showMessage(tr("Fixed image loaded: %1 (%2/%3)")
            .arg(m_fixedImageFiles[index])
            .arg(index + 1)
            .arg(m_fixedImageFiles.size()), 3000);
    } else {
        showError(tr("Error"), tr("Failed to load fixed image."));
    }
    updateActionStates();
}

void MainWindow::loadMovingImageByIndex(int index)
{
    if (index < 0 || index >= m_movingImageFiles.size())
        return;
    
    QString fileName = m_movingImageDir + "/" + m_movingImageFiles[index];
    if (m_imagePairModel->loadMovingImage(fileName)) {
        m_movingImageIndex = index;
        // Update filename label
        ui->lblMovingFileName->setText(tr("%1 (%2/%3)")
            .arg(m_movingImageFiles[index])
            .arg(index + 1)
            .arg(m_movingImageFiles.size()));
        statusBar()->showMessage(tr("Moving image loaded: %1 (%2/%3)")
            .arg(m_movingImageFiles[index])
            .arg(index + 1)
            .arg(m_movingImageFiles.size()), 3000);
    } else {
        showError(tr("Error"), tr("Failed to load moving image."));
    }
    updateActionStates();
}

void MainWindow::prevFixedImage()
{
    if (m_fixedImageIndex <= 0)
        return;
    
    loadFixedImageByIndex(m_fixedImageIndex - 1);
}

void MainWindow::nextFixedImage()
{
    if (m_fixedImageIndex < 0 || m_fixedImageIndex >= m_fixedImageFiles.size() - 1)
        return;
    
    loadFixedImageByIndex(m_fixedImageIndex + 1);
}

void MainWindow::prevMovingImage()
{
    if (m_movingImageIndex <= 0)
        return;
    
    loadMovingImageByIndex(m_movingImageIndex - 1);
}

void MainWindow::nextMovingImage()
{
    if (m_movingImageIndex < 0 || m_movingImageIndex >= m_movingImageFiles.size() - 1)
        return;
    
    loadMovingImageByIndex(m_movingImageIndex + 1);
}

void MainWindow::prevPair()
{
    // Clear current tie points and transform
    m_tiePointModel->clearAll();
    m_hasValidTransform = false;
    ui->txtResult->clear();
    
    prevFixedImage();
    prevMovingImage();
}

void MainWindow::nextPair()
{
    // Clear current tie points and transform
    m_tiePointModel->clearAll();
    m_hasValidTransform = false;
    ui->txtResult->clear();
    
    nextFixedImage();
    nextMovingImage();
}

// ============================================================================
// Point Selection Helpers
// ============================================================================

int MainWindow::findPointAtPosition(QGraphicsView *view, const QPointF &scenePos)
{
    const double hitRadius = 10.0;
    QList<TiePoint> points = m_tiePointModel->getAllTiePoints();
    
    if (view == ui->fixedImageView) {
        for (int i = 0; i < points.size(); ++i) {
            QPointF pointPos = points[i].fixed;
            double dist = QLineF(scenePos, pointPos).length();
            if (dist <= hitRadius) {
                return i;
            }
        }
    } else if (view == ui->movingImageView) {
        for (int i = 0; i < points.size(); ++i) {
            QPointF pointPos = points[i].moving;
            double dist = QLineF(scenePos, pointPos).length();
            if (dist <= hitRadius) {
                return i;
            }
        }
    }
    
    return -1;
}

void MainWindow::handleRubberBandSelection(QGraphicsView *view, const QRect &rubberBandRect)
{
    // Convert rubber band rect to scene coordinates
    QPointF topLeft = view->mapToScene(rubberBandRect.topLeft());
    QPointF bottomRight = view->mapToScene(rubberBandRect.bottomRight());
    QRectF sceneRect(topLeft, bottomRight);
    sceneRect = sceneRect.normalized();
    
    QList<TiePoint> points = m_tiePointModel->getAllTiePoints();
    QItemSelection selection;
    
    for (int i = 0; i < points.size(); ++i) {
        QPointF pointPos;
        if (view == ui->fixedImageView) {
            pointPos = points[i].fixed;
        } else {
            pointPos = points[i].moving;
        }
        
        if (sceneRect.contains(pointPos)) {
            QModelIndex index = m_tiePointModel->index(i, 0);
            selection.select(index, index);
        }
    }
    
    if (!selection.isEmpty()) {
        ui->tiePointsTable->selectionModel()->select(selection, 
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
}

// ============================================================================
// Undo/Redo
// ============================================================================

void MainWindow::undo()
{
    // Try model's internal undo first (for individual point undo)
    if (m_tiePointModel->canUndo()) {
        m_tiePointModel->undoLastPoint();
        m_hasValidTransform = false;
        updatePointDisplay();
        updateActionStates();
        statusBar()->showMessage(tr("Undo: Last point removed"), 2000);
        return;
    }
    
    // Fallback to QUndoStack for other operations (like remove)
    if (m_undoStack->canUndo()) {
        m_undoStack->undo();
        m_hasValidTransform = false;
        updateActionStates();
    }
}

void MainWindow::redo()
{
    // Try model's internal redo first
    if (m_tiePointModel->canRedo()) {
        m_tiePointModel->redoLastPoint();
        m_hasValidTransform = false;
        updatePointDisplay();
        updateActionStates();
        statusBar()->showMessage(tr("Redo: Point restored"), 2000);
        return;
    }
    
    // Fallback to QUndoStack for other operations
    if (m_undoStack->canRedo()) {
        m_undoStack->redo();
        m_hasValidTransform = false;
        updateActionStates();
    }
}

// ============================================================================
// Language Switching
// ============================================================================

void MainWindow::switchToEnglish()
{
    if (m_currentLanguage == "en")
        return;
    
    m_currentLanguage = "en";
    qApp->removeTranslator(m_translator);
    
    // Update action states
    ui->actionLangEnglish->setChecked(true);
    ui->actionLangChinese->setChecked(false);
    
    // Retranslate UI
    ui->retranslateUi(this);
    
    // Update dynamic texts
    m_backendStatusLabel->setText(tr("Backend: Checking..."));
    m_pointCountLabel->setText(tr("Points: %1").arg(m_tiePointModel->count()));
    m_zoomLabel->setText(tr("Zoom: %1%").arg(int(m_zoomFactor * 100)));
    ui->lblPointCount->setText(tr("Points: %1 (min 2 required)").arg(m_tiePointModel->count()));
    
    statusBar()->showMessage(tr("Language switched to English"), 2000);
}

void MainWindow::switchToChinese()
{
    if (m_currentLanguage == "zh")
        return;
    
    m_currentLanguage = "zh";
    
    // Try to load translation file
    QString translationPath = QCoreApplication::applicationDirPath() + "/translations/rigidlabeler_zh.qm";
    if (!QFile::exists(translationPath)) {
        // Try relative path
        translationPath = ":/translations/rigidlabeler_zh.qm";
    }
    
    if (m_translator->load(translationPath) || m_translator->load("rigidlabeler_zh", ":/translations")) {
        qApp->installTranslator(m_translator);
    }
    
    // Update action states
    ui->actionLangEnglish->setChecked(false);
    ui->actionLangChinese->setChecked(true);
    
    // Retranslate UI
    ui->retranslateUi(this);
    
    // Update dynamic texts
    m_backendStatusLabel->setText(tr("Backend: Checking..."));
    m_pointCountLabel->setText(tr("Points: %1").arg(m_tiePointModel->count()));
    m_zoomLabel->setText(tr("Zoom: %1%").arg(int(m_zoomFactor * 100)));
    ui->lblPointCount->setText(tr("Points: %1 (min 2 required)").arg(m_tiePointModel->count()));
    
    statusBar()->showMessage(tr("Language switched to Chinese"), 2000);
}

// ============================================================================
// Real-time Compute Mode
// ============================================================================

void MainWindow::onRealtimeComputeToggled(bool enabled)
{
    m_realtimeComputeEnabled = enabled;
    
    if (enabled) {
        statusBar()->showMessage(tr("Real-time compute mode enabled. Transform will auto-compute 10s after adding a pair."), 3000);
    } else {
        // Stop any pending timer
        m_realtimeComputeTimer->stop();
        m_realtimeComputePending = false;
        statusBar()->showMessage(tr("Real-time compute mode disabled."), 2000);
    }
}

void MainWindow::onRealtimeComputeTimeout()
{
    if (!m_realtimeComputeEnabled || !m_realtimeComputePending)
        return;
    
    m_realtimeComputePending = false;
    
    // Check if we still have enough points
    int pointCount = m_tiePointModel->completePairCount();
    if (pointCount >= 3) {
        statusBar()->showMessage(tr("Auto-computing transform..."), 2000);
        computeTransform();
    }
}

void MainWindow::onPairCompleted(int pairIndex)
{
    Q_UNUSED(pairIndex);
    
    // If real-time compute mode is enabled and we have 3+ pairs, start timer
    if (m_realtimeComputeEnabled) {
        int pointCount = m_tiePointModel->completePairCount();
        if (pointCount >= 3) {
            // Restart the timer (resets the 10s countdown)
            m_realtimeComputeTimer->start();
            m_realtimeComputePending = true;
            statusBar()->showMessage(tr("Pair #%1 complete. Auto-compute in 10 seconds...").arg(pairIndex + 1), 3000);
        }
    }
}

void MainWindow::updateRealtimeComputeState()
{
    int pointCount = m_tiePointModel->completePairCount();
    bool canEnable = pointCount >= 3;
    
    // Enable/disable the checkbox based on point count
    ui->chkRealtimeCompute->setEnabled(canEnable);
    
    // If we drop below 3 points while enabled, auto-disable
    if (!canEnable && m_realtimeComputeEnabled) {
        ui->chkRealtimeCompute->setChecked(false);
        m_realtimeComputeEnabled = false;
        m_realtimeComputeTimer->stop();
        m_realtimeComputePending = false;
        statusBar()->showMessage(tr("Real-time compute mode auto-disabled (less than 3 complete pairs)."), 3000);
    }
}

// ============================================================================
// Matrix Export
// ============================================================================

void MainWindow::exportMatrix()
{
    if (!m_hasValidTransform) {
        showError(tr("Error"), tr("No valid transform to export. Compute transform first."));
        return;
    }
    
    // First time: ask user to select a folder
    if (m_matrixExportDir.isEmpty()) {
        QString dir = QFileDialog::getExistingDirectory(
            this,
            tr("Select Matrix Export Folder"),
            QString(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
        
        if (dir.isEmpty())
            return;
        
        m_matrixExportDir = dir;
    }
    
    // Get filename from fixed image name
    QString fixedPath = m_imagePairModel->fixedImagePath();
    QString baseName;
    if (!fixedPath.isEmpty()) {
        QFileInfo fi(fixedPath);
        baseName = fi.baseName();
    } else {
        baseName = "matrix";
    }
    
    QString filePath = m_matrixExportDir + "/" + baseName + ".txt";
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(tr("Error"), tr("Failed to create file: %1").arg(filePath));
        return;
    }
    
    QTextStream out(&file);
    out.setRealNumberPrecision(10);
    
    // Write 3x3 matrix (space-separated values, one row per line)
    for (int i = 0; i < 3; ++i) {
        out << m_currentMatrix[i][0] << " " << m_currentMatrix[i][1] << " " << m_currentMatrix[i][2] << "\n";
    }
    
    file.close();
    showSuccessToast(tr("Saved successfully"));
}

// ============================================================================
// Tie Point Import/Export
// ============================================================================

void MainWindow::exportTiePoints()
{
    if (m_tiePointModel->completePairCount() == 0) {
        showError(tr("Export Error"), tr("No complete tie points to export."));
        return;
    }
    
    // Default filename from fixed image
    QString defaultFileName;
    QString fixedPath = m_imagePairModel->fixedImagePath();
    if (!fixedPath.isEmpty()) {
        QFileInfo fi(fixedPath);
        defaultFileName = fi.baseName() + ".csv";
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Export Tie Points"),
        defaultFileName,
        tr("CSV Files (*.csv);;All Files (*)")
    );
    
    if (fileName.isEmpty())
        return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(tr("Export Error"), tr("Cannot open file for writing: %1").arg(fileName));
        return;
    }
    
    QTextStream out(&file);
    
    // Get image dimensions for center origin conversion
    double fixedCenterX = 0, fixedCenterY = 0;
    double movingCenterX = 0, movingCenterY = 0;
    
    if (m_fixedPixmapItem) {
        QPixmap pm = m_fixedPixmapItem->pixmap();
        fixedCenterX = pm.width() / 2.0;
        fixedCenterY = pm.height() / 2.0;
    }
    if (m_movingPixmapItem) {
        QPixmap pm = m_movingPixmapItem->pixmap();
        movingCenterX = pm.width() / 2.0;
        movingCenterY = pm.height() / 2.0;
    }
    
    // Write header with origin mode info
    out << "# Tie Points Export\n";
    out << "# Origin Mode: " << (m_useTopLeftOrigin ? "TopLeft" : "Center") << "\n";
    if (!m_useTopLeftOrigin) {
        out << "# Fixed Image Center: " << fixedCenterX << ", " << fixedCenterY << "\n";
        out << "# Moving Image Center: " << movingCenterX << ", " << movingCenterY << "\n";
    }
    out << "# Format: index, fixed_x, fixed_y, moving_x, moving_y\n";
    
    QList<TiePointPair> pairs = m_tiePointModel->getCompletePairs();
    int index = 1;
    for (const TiePointPair &pair : pairs) {
        double fx = pair.fixed->x();
        double fy = pair.fixed->y();
        double mx = pair.moving->x();
        double my = pair.moving->y();
        
        // Convert to center origin if needed
        if (!m_useTopLeftOrigin) {
            fx -= fixedCenterX;
            fy -= fixedCenterY;
            mx -= movingCenterX;
            my -= movingCenterY;
        }
        
        out << index << "," << fx << "," << fy << "," << mx << "," << my << "\n";
        index++;
    }
    
    file.close();
    statusBar()->showMessage(tr("Exported %1 tie points to %2").arg(pairs.size()).arg(fileName), 3000);
}

void MainWindow::importTiePoints()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Import Tie Points"),
        QString(),
        tr("CSV Files (*.csv);;All Files (*)")
    );
    
    if (fileName.isEmpty())
        return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        showError(tr("Import Error"), tr("Cannot open file for reading: %1").arg(fileName));
        return;
    }
    
    // Get image dimensions for center origin conversion
    double fixedCenterX = 0, fixedCenterY = 0;
    double movingCenterX = 0, movingCenterY = 0;
    
    if (m_fixedPixmapItem) {
        QPixmap pm = m_fixedPixmapItem->pixmap();
        fixedCenterX = pm.width() / 2.0;
        fixedCenterY = pm.height() / 2.0;
    }
    if (m_movingPixmapItem) {
        QPixmap pm = m_movingPixmapItem->pixmap();
        movingCenterX = pm.width() / 2.0;
        movingCenterY = pm.height() / 2.0;
    }
    
    QTextStream in(&file);
    int importedCount = 0;
    bool fileUsesCenter = false;  // Detect from file header
    bool hasOriginInfo = false;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        // Skip empty lines
        if (line.isEmpty())
            continue;
        
        // Parse header comments
        if (line.startsWith('#')) {
            if (line.contains("Origin Mode:")) {
                hasOriginInfo = true;
                fileUsesCenter = line.contains("Center");
            }
            continue;
        }
        
        // Parse data line: index, fixed_x, fixed_y, moving_x, moving_y
        QStringList parts = line.split(',');
        if (parts.size() < 5)
            continue;
        
        bool ok1, ok2, ok3, ok4;
        double fx = parts[1].trimmed().toDouble(&ok1);
        double fy = parts[2].trimmed().toDouble(&ok2);
        double mx = parts[3].trimmed().toDouble(&ok3);
        double my = parts[4].trimmed().toDouble(&ok4);
        
        if (!ok1 || !ok2 || !ok3 || !ok4)
            continue;
        
        // Convert coordinates based on file origin mode and current mode
        // File uses center origin -> convert to pixel coords if current mode is top-left
        // File uses top-left origin -> convert to center coords if current mode is center
        if (hasOriginInfo && fileUsesCenter) {
            // File is in center coords, convert to top-left (pixel) coords first
            fx += fixedCenterX;
            fy += fixedCenterY;
            mx += movingCenterX;
            my += movingCenterY;
        }
        
        // Now fx, fy, mx, my are in pixel (top-left) coordinates
        // Add the point to model (model always stores in pixel coords)
        m_tiePointModel->addTiePoint(QPointF(fx, fy), QPointF(mx, my));
        importedCount++;
    }
    
    file.close();
    
    if (importedCount > 0) {
        updatePointDisplay();
        updateActionStates();
        statusBar()->showMessage(tr("Imported %1 tie points from %2").arg(importedCount).arg(fileName), 3000);
    } else {
        showError(tr("Import Error"), tr("No valid tie points found in file."));
    }
}

void MainWindow::onOriginModeToggled(bool topLeftOrigin)
{
    m_useTopLeftOrigin = topLeftOrigin;
    
    // Update TiePointModel's display mode
    m_tiePointModel->setUseTopLeftOrigin(topLeftOrigin);
    
    if (topLeftOrigin) {
        statusBar()->showMessage(tr("Coordinate origin: Top-left corner (0,0)"), 2000);
    } else {
        statusBar()->showMessage(tr("Coordinate origin: Image center (0,0)"), 2000);
    }
    
    // Note: Internal storage is always in pixel coords, only display/export changes
}

void MainWindow::updateTiePointModelCoordinateOffsets()
{
    QPointF fixedOffset(0, 0);
    QPointF movingOffset(0, 0);
    
    if (m_fixedPixmapItem) {
        QPixmap pm = m_fixedPixmapItem->pixmap();
        fixedOffset = QPointF(pm.width() / 2.0, pm.height() / 2.0);
    }
    
    if (m_movingPixmapItem) {
        QPixmap pm = m_movingPixmapItem->pixmap();
        movingOffset = QPointF(pm.width() / 2.0, pm.height() / 2.0);
    }
    
    m_tiePointModel->setDisplayCoordinateOffset(fixedOffset, movingOffset);
}
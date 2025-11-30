#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "model/TiePointModel.h"
#include "model/ImagePairModel.h"
#include "app/BackendClient.h"
#include "app/AppConfig.h"

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
    , m_gtExportCounter(0)
    , m_isPanning(false)
    , m_isSelecting(false)
    , m_fixedRubberBand(nullptr)
    , m_movingRubberBand(nullptr)
    , m_undoStack(new QUndoStack(this))
    , m_translator(new QTranslator(this))
    , m_currentLanguage("en")
{
    ui->setupUi(this);
    
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
    connect(ui->actionFitToWindow, &QAction::triggered, this, &MainWindow::zoomToFit);
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
    connect(ui->btnNextFixed, &QPushButton::clicked, this, &MainWindow::nextFixedImage);
    connect(ui->btnNextMoving, &QPushButton::clicked, this, &MainWindow::nextMovingImage);
    connect(ui->btnNextPair, &QPushButton::clicked, this, &MainWindow::nextPair);
    connect(ui->btnZoomFitFixed, &QPushButton::clicked, this, &MainWindow::zoomToFit);
    connect(ui->btnZoomFitMoving, &QPushButton::clicked, this, &MainWindow::zoomToFit);
    connect(ui->btnAddPoint, &QPushButton::clicked, this, &MainWindow::addTiePoint);
    connect(ui->btnDeletePoint, &QPushButton::clicked, this, &MainWindow::deleteSelectedTiePoint);
    connect(ui->btnClearPoints, &QPushButton::clicked, this, &MainWindow::clearAllTiePoints);
    connect(ui->btnCompute, &QPushButton::clicked, this, &MainWindow::computeTransform);
    connect(ui->btnSaveLabel, &QPushButton::clicked, this, &MainWindow::saveLabel);
    connect(ui->btnLoadLabel, &QPushButton::clicked, this, &MainWindow::loadLabel);
    connect(ui->btnPreview, &QPushButton::clicked, this, &MainWindow::previewWarp);
    connect(ui->btnExportGT, &QPushButton::clicked, this, &MainWindow::exportToGTFolder);
    
    // Options
    connect(ui->chkShowOverlay, &QCheckBox::toggled, this, &MainWindow::onOverlayToggled);
    connect(ui->sliderOpacity, &QSlider::valueChanged, this, &MainWindow::onOpacityChanged);
    
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
    rigid.scale = m_currentScale;
    
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
    
    // Collect complete tie points only
    QList<QPair<QPointF, QPointF>> tiePoints;
    for (const TiePoint &tp : m_tiePointModel->getAllTiePoints()) {
        tiePoints.append({tp.fixed, tp.moving});
    }
    
    m_backendClient->computeRigid(
        tiePoints,
        ui->chkAllowScale->isChecked(),
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
    
    // TODO: Implement warp preview using backend /warp/preview endpoint
    statusBar()->showMessage(tr("Preview warp - not yet implemented."), 3000);
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

void MainWindow::zoomToFit()
{
    if (m_fixedPixmapItem) {
        ui->fixedImageView->fitInView(m_fixedPixmapItem, Qt::KeepAspectRatio);
    }
    if (m_movingPixmapItem) {
        ui->movingImageView->fitInView(m_movingPixmapItem, Qt::KeepAspectRatio);
    }
    m_zoomFactor = 1.0;
    m_zoomLabel->setText(tr("Zoom: Fit"));
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
    
    ui->lblFixedCoord->setText(tr("X: %1, Y: %2").arg(pos.x(), 0, 'f', 1).arg(pos.y(), 0, 'f', 1));
    
    // Update display
    updatePointDisplay();
    updateActionStates();
    
    // Check if this completes a pair
    if (m_tiePointModel->hasBothPoints(index)) {
        statusBar()->showMessage(tr("Fixed point added at (%1, %2). Pair #%3 complete!")
            .arg(pos.x(), 0, 'f', 1).arg(pos.y(), 0, 'f', 1).arg(index + 1), 3000);
        m_hasValidTransform = false;
    } else {
        statusBar()->showMessage(tr("Fixed point added at (%1, %2). Now add moving point for pair #%3.")
            .arg(pos.x(), 0, 'f', 1).arg(pos.y(), 0, 'f', 1).arg(index + 1), 5000);
    }
}

void MainWindow::onMovingViewClicked(const QPointF &pos)
{
    // Clear cursor marker first
    clearCursorMarker();
    
    // Add moving point to model (model handles internal undo stack)
    int index = m_tiePointModel->addMovingPoint(pos);
    
    ui->lblMovingCoord->setText(tr("X: %1, Y: %2").arg(pos.x(), 0, 'f', 1).arg(pos.y(), 0, 'f', 1));
    
    // Update display
    updatePointDisplay();
    updateActionStates();
    
    // Check if this completes a pair
    if (m_tiePointModel->hasBothPoints(index)) {
        statusBar()->showMessage(tr("Moving point added at (%1, %2). Pair #%3 complete!")
            .arg(pos.x(), 0, 'f', 1).arg(pos.y(), 0, 'f', 1).arg(index + 1), 3000);
        m_hasValidTransform = false;
    } else {
        statusBar()->showMessage(tr("Moving point added at (%1, %2). Now add fixed point for pair #%3.")
            .arg(pos.x(), 0, 'f', 1).arg(pos.y(), 0, 'f', 1).arg(index + 1), 5000);
    }
}

// ============================================================================
// Options
// ============================================================================

void MainWindow::onOverlayToggled(bool checked)
{
    // TODO: Implement overlay visualization
    Q_UNUSED(checked);
}

void MainWindow::onOpacityChanged(int value)
{
    ui->lblOpacityValue->setText(tr("%1%").arg(value));
    // TODO: Update overlay opacity
}

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
    m_currentScale = result.rigid.scale;
    m_currentMatrix = result.matrix3x3;
    
    // Display results
    QString resultText;
    resultText += tr("Rotation: %1°\n").arg(result.rigid.theta_deg, 0, 'f', 4);
    resultText += tr("Translation: (%1, %2)\n").arg(result.rigid.tx, 0, 'f', 4).arg(result.rigid.ty, 0, 'f', 4);
    resultText += tr("Scale: %1\n").arg(result.rigid.scale, 0, 'f', 6);
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
    m_currentScale = result.rigid.scale;
    m_currentMatrix = result.matrix3x3;
    
    // Display results
    QString resultText;
    resultText += tr("Rotation: %1°\n").arg(result.rigid.theta_deg, 0, 'f', 4);
    resultText += tr("Translation: (%1, %2)\n").arg(result.rigid.tx, 0, 'f', 4).arg(result.rigid.ty, 0, 'f', 4);
    resultText += tr("Scale: %1\n").arg(result.rigid.scale, 0, 'f', 6);
    resultText += tr("\n(Loaded from saved label)");
    
    ui->txtResult->setText(resultText);
    statusBar()->showMessage(tr("Label loaded successfully."), 3000);
    updateActionStates();
}

// ============================================================================
// About Dialog
// ============================================================================

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About RigidLabeler"),
        tr("<h2>RigidLabeler</h2>"
           "<p>A 2D rigid transformation labeling tool.</p>"
           "<p>Version 1.0</p>"
           "<p>This tool allows you to:</p>"
           "<ul>"
           "<li>Load image pairs (fixed and moving)</li>"
           "<li>Define tie points between images</li>"
           "<li>Compute rigid/similarity transforms</li>"
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
    int minPoints = AppConfig::instance().minPointsRequired();
    
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
    
    // Navigation buttons
    ui->btnNextFixed->setEnabled(m_fixedImageIndex >= 0 && m_fixedImageIndex < m_fixedImageFiles.size() - 1);
    ui->btnNextMoving->setEnabled(m_movingImageIndex >= 0 && m_movingImageIndex < m_movingImageFiles.size() - 1);
    ui->btnNextPair->setEnabled(ui->btnNextFixed->isEnabled() || ui->btnNextMoving->isEnabled());
    
    // GT export button
    ui->btnExportGT->setEnabled(m_hasValidTransform);
}

void MainWindow::showError(const QString &title, const QString &message)
{
    QMessageBox::critical(this, title, message);
}

void MainWindow::showInfo(const QString &title, const QString &message)
{
    QMessageBox::information(this, title, message);
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
        statusBar()->showMessage(tr("Moving image loaded: %1 (%2/%3)")
            .arg(m_movingImageFiles[index])
            .arg(index + 1)
            .arg(m_movingImageFiles.size()), 3000);
    } else {
        showError(tr("Error"), tr("Failed to load moving image."));
    }
    updateActionStates();
}

void MainWindow::nextFixedImage()
{
    if (m_fixedImageIndex < 0 || m_fixedImageIndex >= m_fixedImageFiles.size() - 1)
        return;
    
    loadFixedImageByIndex(m_fixedImageIndex + 1);
}

void MainWindow::nextMovingImage()
{
    if (m_movingImageIndex < 0 || m_movingImageIndex >= m_movingImageFiles.size() - 1)
        return;
    
    loadMovingImageByIndex(m_movingImageIndex + 1);
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
// GT Export
// ============================================================================

void MainWindow::exportToGTFolder()
{
    if (!m_hasValidTransform) {
        showError(tr("Error"), tr("No valid transform to export. Compute transform first."));
        return;
    }
    
    // First time: select root folder
    if (m_gtExportRootDir.isEmpty()) {
        QString dir = QFileDialog::getExistingDirectory(this,
            tr("Select GT Export Root Folder"),
            AppConfig::instance().lastGTExportDir(),
            QFileDialog::ShowDirsOnly);
        
        if (dir.isEmpty())
            return;
        
        m_gtExportRootDir = dir;
        m_gtExportCounter = 0;
        AppConfig::instance().setLastGTExportDir(dir);
    }
    
    // Create GT subfolder if it doesn't exist
    QString gtFolderPath = m_gtExportRootDir + "/GT";
    QDir gtDir(gtFolderPath);
    if (!gtDir.exists()) {
        if (!gtDir.mkpath(".")) {
            showError(tr("Error"), tr("Failed to create GT folder: %1").arg(gtFolderPath));
            return;
        }
    }
    
    // Find next available counter
    while (QFile::exists(gtFolderPath + QString("/%1.txt").arg(m_gtExportCounter, 4, 10, QChar('0')))) {
        m_gtExportCounter++;
    }
    
    // Write 3x3 matrix to txt file
    QString fileName = gtFolderPath + QString("/%1.txt").arg(m_gtExportCounter, 4, 10, QChar('0'));
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(tr("Error"), tr("Failed to create file: %1").arg(fileName));
        return;
    }
    
    QTextStream out(&file);
    out.setRealNumberPrecision(10);
    for (int i = 0; i < 3; ++i) {
        out << m_currentMatrix[i][0] << " " << m_currentMatrix[i][1] << " " << m_currentMatrix[i][2] << "\n";
    }
    file.close();
    
    statusBar()->showMessage(tr("Exported to: %1").arg(fileName), 3000);
    m_gtExportCounter++;
    
    // Auto-advance to next pair if possible
    if (ui->btnNextPair->isEnabled()) {
        int ret = QMessageBox::question(this, tr("Next Pair?"),
            tr("Matrix exported successfully. Load next image pair?"),
            QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            nextPair();
        }
    }
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
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPointF>
#include <QVector>
#include <QStringList>
#include <QRubberBand>
#include <QUndoStack>
#include <QTranslator>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class TiePointModel;
class ImagePairModel;
class BackendClient;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QGraphicsEllipseItem;
class QGraphicsView;
class QGraphicsItemGroup;
class QGraphicsLineItem;
class PreviewDialog;

struct ComputeRigidResult;
struct LabelSaveResult;
struct LabelData;
struct HealthCheckResult;
struct CheckerboardPreviewResult;

/**
 * @brief Main application window for RigidLabeler.
 * 
 * Provides the main UI for:
 * - Loading and displaying image pairs
 * - Adding and managing tie points
 * - Computing rigid transformations
 * - Saving and loading labels
 */

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // File operations
    void loadFixedImage();
    void loadMovingImage();
    void clearImages();
    
    // Navigation operations
    void prevFixedImage();
    void nextFixedImage();
    void prevMovingImage();
    void nextMovingImage();
    void prevPair();
    void nextPair();
    
    // Label operations
    void saveLabel();
    void loadLabel();
    
    // Transform operations
    void computeTransform();
    void previewWarp();
    
    // View operations
    void zoomIn();
    void zoomOut();
    void zoomToFitFixed();
    void zoomToFitMoving();
    void zoomToFitAll();
    void toggleLinkViews(bool linked);
    
    // Tie point operations
    void addTiePoint();
    void deleteSelectedTiePoint();
    void clearAllTiePoints();
    void onTiePointSelectionChanged();
    void exportTiePoints();
    void importTiePoints();
    void exportMatrix();
    void quickExportTiePoints();  // Ctrl+S: auto export tie points
    void onOriginModeToggled(bool topLeftOrigin);
    
    // Image view interactions
    void onFixedViewClicked(const QPointF &pos);
    void onMovingViewClicked(const QPointF &pos);
    
    // Backend responses
    void onHealthCheckCompleted(const HealthCheckResult &result);
    void onComputeRigidCompleted(const ComputeRigidResult &result);
    void onSaveLabelCompleted(const LabelSaveResult &result);
    void onLoadLabelCompleted(const LabelData &result);
    void onCheckerboardPreviewCompleted(const CheckerboardPreviewResult &result);
    
    // About dialog
    void showAbout();
    
    // Undo/Redo
    void undo();
    void redo();
    
    // Language
    void switchToEnglish();
    void switchToChinese();
    
    // Real-time compute mode
    void onRealtimeComputeToggled(bool enabled);
    void onRealtimeComputeTimeout();
    void onPairCompleted(int pairIndex);
    void updateRealtimeComputeState();
    
    // Preview
    void onPreviewRefreshRequested(int gridSize);

private:
    void setupConnections();
    void setupImageViews();
    void installEventFilters();
    
    void updateImageViews();
    void updateTiePointViews();
    void updatePointDisplay();
    void updateActionStates();
    
    void showError(const QString &title, const QString &message);
    void showInfo(const QString &title, const QString &message);
    void showSuccessToast(const QString &message, int durationMs = 2000);
    
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEventOnView(QGraphicsView *view, QWheelEvent *event);
    
    // Coordinate conversion helpers
    QPointF pixelToDisplayCoord(const QPointF &pixelPos, bool isFixed) const;
    QString formatDisplayCoord(const QPointF &pixelPos, bool isFixed) const;
    void updateTiePointModelCoordinateOffsets();
    
    // Image navigation helpers
    QStringList getImageFilesInDir(const QString &dir);
    void loadFixedImageByIndex(int index);
    void loadMovingImageByIndex(int index);
    
    // Mouse interaction helpers
    void handleRubberBandSelection(QGraphicsView *view, const QRect &rubberBandRect);
    int findPointAtPosition(QGraphicsView *view, const QPointF &scenePos);
    
    // Crosshair marker helpers
    QGraphicsItemGroup* createCrosshairMarker(QGraphicsScene *scene, const QPointF &pos, 
                                               const QColor &color, bool highlighted = false,
                                               int pointIndex = -1);
    void updatePendingPointMarker();
    void clearPendingPointMarker();
    void updateCursorMarker(QGraphicsScene *scene, const QPointF &pos);
    void clearCursorMarker();
    QColor getNextPointColor() const;
    
    // Project cache helpers
    void saveProjectState();
    void restoreLastProject();
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;
    
    // Models
    TiePointModel *m_tiePointModel;
    ImagePairModel *m_imagePairModel;
    
    // Backend client
    BackendClient *m_backendClient;
    
    // Graphics scenes for image views
    QGraphicsScene *m_fixedScene;
    QGraphicsScene *m_movingScene;
    QGraphicsPixmapItem *m_fixedPixmapItem;
    QGraphicsPixmapItem *m_movingPixmapItem;
    
    // Point markers on scenes (crosshair groups)
    QList<QGraphicsItemGroup*> m_fixedPointMarkers;
    QList<QGraphicsItemGroup*> m_movingPointMarkers;
    
    // Pending point marker (shown when first point clicked on fixed image)
    QGraphicsItemGroup *m_pendingPointMarker;
    
    // Mouse-following crosshair marker
    QGraphicsItemGroup *m_cursorMarker;
    QGraphicsScene *m_cursorMarkerScene;  // Which scene the cursor marker is in
    
    // Color palette for point pairs
    QList<QColor> m_pointColors;
    
    // Status bar labels
    QLabel *m_backendStatusLabel;
    QLabel *m_pointCountLabel;
    QLabel *m_zoomLabel;
    
    // Current transform result
    bool m_hasValidTransform;
    QVector<QVector<double>> m_currentMatrix;
    double m_currentTheta;
    double m_currentTx;
    double m_currentTy;
    double m_currentScaleX;
    double m_currentScaleY;
    double m_currentShear;
    
    // Point adding mode
    bool m_isAddingPoint;
    
    // Current zoom level
    double m_zoomFactor;
    
    // Image folder navigation
    QString m_fixedImageDir;
    QStringList m_fixedImageFiles;
    int m_fixedImageIndex;
    QString m_movingImageDir;
    QStringList m_movingImageFiles;
    int m_movingImageIndex;
    
    // Matrix export
    QString m_matrixExportDir;
    
    // Tie points auto export
    QString m_tiePointsExportDir;
    
    // Mouse interaction state
    bool m_isPanning;
    bool m_isSelecting;
    QPoint m_lastPanPos;
    QRubberBand *m_fixedRubberBand;
    QRubberBand *m_movingRubberBand;
    QPoint m_rubberBandOrigin;
    
    // Selected point indices (for multi-selection)
    QSet<int> m_selectedPointIndices;
    
    // Undo/Redo
    QUndoStack *m_undoStack;
    
    // Language/Translation
    QTranslator *m_translator;
    QString m_currentLanguage;
    
    // Real-time compute mode
    QTimer *m_realtimeComputeTimer;
    bool m_realtimeComputeEnabled;
    bool m_realtimeComputePending;  // A compute is pending after timer expires
    
    // Coordinate origin mode (false = center origin, true = top-left origin)
    bool m_useTopLeftOrigin;
    
    // Preview dialog
    PreviewDialog *m_previewDialog;
    int m_currentPreviewGridSize;
    
    // Point label display mode
    bool m_showPointLabels;
};

#endif // MAINWINDOW_H

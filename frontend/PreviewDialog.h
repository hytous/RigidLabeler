#ifndef PREVIEWDIALOG_H
#define PREVIEWDIALOG_H

#include <QDialog>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QImage>

class QGraphicsView;
class QSlider;
class QLabel;
class QSpinBox;
class QPushButton;

/**
 * @brief Dialog for displaying checkerboard preview of warped images.
 * 
 * Shows a checkerboard pattern combining the fixed image and the 
 * transformed moving image. Allows adjusting the grid density.
 */
class PreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreviewDialog(QWidget *parent = nullptr);
    ~PreviewDialog();

    /**
     * @brief Set the preview image from base64-encoded PNG data.
     * @param base64Data Base64-encoded PNG image data.
     * @param width Image width in pixels.
     * @param height Image height in pixels.
     * @return true if image was loaded successfully.
     */
    bool setImageFromBase64(const QString &base64Data, int width, int height);

    /**
     * @brief Get current grid size (number of cells per row/column).
     */
    int gridSize() const;

    /**
     * @brief Set the grid size.
     */
    void setGridSize(int size);

signals:
    /**
     * @brief Emitted when user requests to refresh the preview with new grid size.
     * @param gridSize New grid size to use.
     */
    void refreshRequested(int gridSize);

public slots:
    /**
     * @brief Show loading state while waiting for preview.
     */
    void showLoading();

    /**
     * @brief Show error message in the preview area.
     */
    void showError(const QString &message);

private slots:
    void onGridSizeChanged(int value);
    void onRefreshClicked();
    void onZoomIn();
    void onZoomOut();
    void onZoomFit();
    void onSaveImage();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUi();
    void updateZoomLabel();

    QGraphicsView *m_graphicsView;
    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;
    
    QSpinBox *m_gridSizeSpinBox;
    QPushButton *m_refreshButton;
    QPushButton *m_zoomInButton;
    QPushButton *m_zoomOutButton;
    QPushButton *m_zoomFitButton;
    QPushButton *m_saveButton;
    QLabel *m_zoomLabel;
    QLabel *m_statusLabel;
    
    QImage m_currentImage;
    double m_zoomFactor;
};

#endif // PREVIEWDIALOG_H

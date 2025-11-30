#include "PreviewDialog.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QByteArray>
#include <QBuffer>
#include <QWheelEvent>

PreviewDialog::PreviewDialog(QWidget *parent)
    : QDialog(parent)
    , m_graphicsView(nullptr)
    , m_scene(new QGraphicsScene(this))
    , m_pixmapItem(nullptr)
    , m_gridSizeSpinBox(nullptr)
    , m_refreshButton(nullptr)
    , m_zoomInButton(nullptr)
    , m_zoomOutButton(nullptr)
    , m_zoomFitButton(nullptr)
    , m_saveButton(nullptr)
    , m_zoomLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_zoomFactor(1.0)
{
    setupUi();
    setWindowTitle(tr("Checkerboard Preview"));
    resize(800, 600);
}

PreviewDialog::~PreviewDialog()
{
}

void PreviewDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Top toolbar
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    
    // Grid size control
    QLabel *gridLabel = new QLabel(tr("Grid Size:"));
    m_gridSizeSpinBox = new QSpinBox();
    m_gridSizeSpinBox->setRange(2, 64);
    m_gridSizeSpinBox->setValue(8);
    m_gridSizeSpinBox->setToolTip(tr("Number of grid cells per row/column (2-64)"));
    
    m_refreshButton = new QPushButton(tr("Refresh"));
    m_refreshButton->setToolTip(tr("Regenerate preview with new grid size"));
    
    toolbarLayout->addWidget(gridLabel);
    toolbarLayout->addWidget(m_gridSizeSpinBox);
    toolbarLayout->addWidget(m_refreshButton);
    
    toolbarLayout->addSpacing(20);
    
    // Zoom controls
    m_zoomInButton = new QPushButton(tr("+"));
    m_zoomInButton->setFixedWidth(30);
    m_zoomInButton->setToolTip(tr("Zoom In"));
    
    m_zoomOutButton = new QPushButton(tr("-"));
    m_zoomOutButton->setFixedWidth(30);
    m_zoomOutButton->setToolTip(tr("Zoom Out"));
    
    m_zoomFitButton = new QPushButton(tr("Fit"));
    m_zoomFitButton->setToolTip(tr("Fit to Window"));
    
    m_zoomLabel = new QLabel(tr("Zoom: 100%"));
    
    toolbarLayout->addWidget(m_zoomOutButton);
    toolbarLayout->addWidget(m_zoomInButton);
    toolbarLayout->addWidget(m_zoomFitButton);
    toolbarLayout->addWidget(m_zoomLabel);
    
    toolbarLayout->addStretch();
    
    // Save button
    m_saveButton = new QPushButton(tr("Save Image"));
    m_saveButton->setToolTip(tr("Save preview image to file"));
    toolbarLayout->addWidget(m_saveButton);
    
    mainLayout->addLayout(toolbarLayout);
    
    // Graphics view for image display
    m_graphicsView = new QGraphicsView(m_scene);
    m_graphicsView->setRenderHint(QPainter::Antialiasing);
    m_graphicsView->setRenderHint(QPainter::SmoothPixmapTransform);
    m_graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
    m_graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_graphicsView->setBackgroundBrush(QBrush(Qt::darkGray));
    m_graphicsView->viewport()->installEventFilter(this);
    mainLayout->addWidget(m_graphicsView, 1);
    
    // Status bar
    m_statusLabel = new QLabel();
    mainLayout->addWidget(m_statusLabel);
    
    // Connect signals
    connect(m_gridSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PreviewDialog::onGridSizeChanged);
    connect(m_refreshButton, &QPushButton::clicked, this, &PreviewDialog::onRefreshClicked);
    connect(m_zoomInButton, &QPushButton::clicked, this, &PreviewDialog::onZoomIn);
    connect(m_zoomOutButton, &QPushButton::clicked, this, &PreviewDialog::onZoomOut);
    connect(m_zoomFitButton, &QPushButton::clicked, this, &PreviewDialog::onZoomFit);
    connect(m_saveButton, &QPushButton::clicked, this, &PreviewDialog::onSaveImage);
}

bool PreviewDialog::setImageFromBase64(const QString &base64Data, int width, int height)
{
    // Decode base64 data
    QByteArray imageData = QByteArray::fromBase64(base64Data.toUtf8());
    
    // Load image from data
    QImage image;
    if (!image.loadFromData(imageData, "PNG")) {
        showError(tr("Failed to decode image data"));
        return false;
    }
    
    m_currentImage = image;
    
    // Clear existing items
    m_scene->clear();
    m_pixmapItem = nullptr;
    
    // Add new pixmap
    QPixmap pixmap = QPixmap::fromImage(m_currentImage);
    m_pixmapItem = m_scene->addPixmap(pixmap);
    m_scene->setSceneRect(pixmap.rect());
    
    // Fit to view
    onZoomFit();
    
    // Update status
    m_statusLabel->setText(tr("Image: %1 x %2 pixels").arg(width).arg(height));
    
    return true;
}

int PreviewDialog::gridSize() const
{
    return m_gridSizeSpinBox ? m_gridSizeSpinBox->value() : 8;
}

void PreviewDialog::setGridSize(int size)
{
    if (m_gridSizeSpinBox) {
        m_gridSizeSpinBox->setValue(size);
    }
}

void PreviewDialog::showLoading()
{
    // Clear existing items
    m_scene->clear();
    m_pixmapItem = nullptr;
    
    // Add loading text
    QGraphicsTextItem *textItem = m_scene->addText(tr("Loading..."));
    textItem->setDefaultTextColor(Qt::white);
    QFont font = textItem->font();
    font.setPointSize(16);
    textItem->setFont(font);
    
    m_statusLabel->setText(tr("Generating preview..."));
}

void PreviewDialog::showError(const QString &message)
{
    // Clear existing items
    m_scene->clear();
    m_pixmapItem = nullptr;
    
    // Add error text
    QGraphicsTextItem *textItem = m_scene->addText(tr("Error: ") + message);
    textItem->setDefaultTextColor(Qt::red);
    QFont font = textItem->font();
    font.setPointSize(14);
    textItem->setFont(font);
    
    m_statusLabel->setText(tr("Error: ") + message);
}

void PreviewDialog::onGridSizeChanged(int value)
{
    // Just update status, don't auto-refresh
    m_statusLabel->setText(tr("Grid size changed to %1. Click Refresh to update.").arg(value));
}

void PreviewDialog::onRefreshClicked()
{
    emit refreshRequested(m_gridSizeSpinBox->value());
}

void PreviewDialog::onZoomIn()
{
    const double scaleFactor = 1.25;
    m_graphicsView->scale(scaleFactor, scaleFactor);
    m_zoomFactor *= scaleFactor;
    updateZoomLabel();
}

void PreviewDialog::onZoomOut()
{
    const double scaleFactor = 1.25;
    m_graphicsView->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    m_zoomFactor /= scaleFactor;
    updateZoomLabel();
}

void PreviewDialog::onZoomFit()
{
    if (m_pixmapItem) {
        m_graphicsView->fitInView(m_pixmapItem, Qt::KeepAspectRatio);
        
        // Calculate actual zoom factor
        QRectF sceneRect = m_scene->sceneRect();
        QRectF viewRect = m_graphicsView->viewport()->rect();
        double scaleX = viewRect.width() / sceneRect.width();
        double scaleY = viewRect.height() / sceneRect.height();
        m_zoomFactor = qMin(scaleX, scaleY);
        
        updateZoomLabel();
    }
}

void PreviewDialog::updateZoomLabel()
{
    m_zoomLabel->setText(tr("Zoom: %1%").arg(int(m_zoomFactor * 100)));
}

void PreviewDialog::onSaveImage()
{
    if (m_currentImage.isNull()) {
        QMessageBox::warning(this, tr("Warning"), tr("No image to save"));
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Save Checkerboard Image"),
        QString(),
        tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;All Files (*)")
    );
    
    if (fileName.isEmpty()) {
        return;
    }
    
    if (m_currentImage.save(fileName)) {
        m_statusLabel->setText(tr("Image saved: %1").arg(fileName));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save image"));
    }
}

bool PreviewDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_graphicsView->viewport() && event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
        
        // Zoom with mouse wheel
        const double scaleFactor = 1.15;
        if (wheelEvent->angleDelta().y() > 0) {
            // Zoom in
            m_graphicsView->scale(scaleFactor, scaleFactor);
            m_zoomFactor *= scaleFactor;
        } else {
            // Zoom out
            m_graphicsView->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
            m_zoomFactor /= scaleFactor;
        }
        updateZoomLabel();
        return true;
    }
    
    return QDialog::eventFilter(obj, event);
}

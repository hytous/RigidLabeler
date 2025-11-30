#ifndef IMAGEPAIRMODEL_H
#define IMAGEPAIRMODEL_H

#include <QObject>
#include <QString>
#include <QImage>

/**
 * @brief Model for managing a pair of images (fixed and moving).
 * 
 * Handles loading, storing, and providing access to the image pair
 * used for registration labeling.
 */
class ImagePairModel : public QObject
{
    Q_OBJECT

public:
    explicit ImagePairModel(QObject *parent = nullptr);

    // Image loading
    bool loadFixedImage(const QString &path);
    bool loadMovingImage(const QString &path);
    void clearImages();

    // Getters
    QString fixedImagePath() const { return m_fixedPath; }
    QString movingImagePath() const { return m_movingPath; }
    const QImage& fixedImage() const { return m_fixedImage; }
    const QImage& movingImage() const { return m_movingImage; }
    
    bool hasFixedImage() const { return !m_fixedImage.isNull(); }
    bool hasMovingImage() const { return !m_movingImage.isNull(); }
    bool hasBothImages() const { return hasFixedImage() && hasMovingImage(); }

signals:
    void fixedImageChanged(const QString &path);
    void movingImageChanged(const QString &path);
    void imagesCleared();

private:
    QString m_fixedPath;
    QString m_movingPath;
    QImage m_fixedImage;
    QImage m_movingImage;
};

#endif // IMAGEPAIRMODEL_H

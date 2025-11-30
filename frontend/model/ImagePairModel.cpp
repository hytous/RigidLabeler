#include "ImagePairModel.h"
#include <QFileInfo>
#include <QDebug>

ImagePairModel::ImagePairModel(QObject *parent)
    : QObject(parent)
{
}

bool ImagePairModel::loadFixedImage(const QString &path)
{
    QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qWarning() << "Fixed image file does not exist:" << path;
        return false;
    }

    QImage image(path);
    if (image.isNull()) {
        qWarning() << "Failed to load fixed image:" << path;
        return false;
    }

    m_fixedPath = path;
    m_fixedImage = image;
    
    emit fixedImageChanged(path);
    return true;
}

bool ImagePairModel::loadMovingImage(const QString &path)
{
    QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qWarning() << "Moving image file does not exist:" << path;
        return false;
    }

    QImage image(path);
    if (image.isNull()) {
        qWarning() << "Failed to load moving image:" << path;
        return false;
    }

    m_movingPath = path;
    m_movingImage = image;
    
    emit movingImageChanged(path);
    return true;
}

void ImagePairModel::clearImages()
{
    m_fixedPath.clear();
    m_movingPath.clear();
    m_fixedImage = QImage();
    m_movingImage = QImage();
    
    emit imagesCleared();
}

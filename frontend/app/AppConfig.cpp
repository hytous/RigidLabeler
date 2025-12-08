#include "AppConfig.h"
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>

// Simple YAML parsing for our limited use case
// For production, consider using a proper YAML library

AppConfig& AppConfig::instance()
{
    static AppConfig instance;
    return instance;
}

AppConfig::AppConfig()
    : m_backendBaseUrl("http://127.0.0.1:8000")
    , m_defaultImagesRoot("data/images")
    , m_defaultLabelsRoot("data/labels")
    , m_language("zh-CN")
    , m_theme("light")
    , m_linkViewsByDefault(true)
    , m_rememberLastDir(true)
    , m_allowScaleDefault(false)
    , m_minPointsRequired(3)
    , m_settings(new QSettings("RigidLabeler", "Frontend"))
{
}

bool AppConfig::load(const QString &configPath)
{
    QString path = configPath;
    if (path.isEmpty()) {
        // Default config location: ../config/app.yaml relative to executable
        QDir appDir(QCoreApplication::applicationDirPath());
        appDir.cdUp();
        path = appDir.filePath("config/app.yaml");
    }
    
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open config file:" << path;
        qWarning() << "Using default configuration.";
        return false;
    }
    
    // Simple line-by-line YAML parsing
    QString currentSection;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        
        // Skip comments and empty lines
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        
        // Check for section (no leading spaces, ends with :)
        if (!line.startsWith(' ') && line.endsWith(':') && !line.contains('"')) {
            currentSection = line.left(line.length() - 1);
            continue;
        }
        
        // Parse key: value
        int colonPos = line.indexOf(':');
        if (colonPos <= 0)
            continue;
        
        QString key = line.left(colonPos).trimmed();
        QString value = line.mid(colonPos + 1).trimmed();
        
        // Remove quotes if present
        if (value.startsWith('"') && value.endsWith('"')) {
            value = value.mid(1, value.length() - 2);
        }
        
        // Apply to configuration
        if (currentSection == "backend") {
            if (key == "base_url") m_backendBaseUrl = value;
        }
        else if (currentSection == "paths") {
            if (key == "default_images_root") m_defaultImagesRoot = value;
            else if (key == "default_labels_root") m_defaultLabelsRoot = value;
        }
        else if (currentSection == "ui") {
            if (key == "language") m_language = value;
            else if (key == "theme") m_theme = value;
            else if (key == "link_views_by_default") m_linkViewsByDefault = (value == "true");
            else if (key == "remember_last_dir") m_rememberLastDir = (value == "true");
        }
        else if (currentSection == "transform") {
            if (key == "allow_scale_default") m_allowScaleDefault = (value == "true");
            else if (key == "min_points_required") m_minPointsRequired = value.toInt();
        }
    }
    
    file.close();
    qDebug() << "Configuration loaded from:" << path;
    return true;
}

QString AppConfig::lastFixedImageDir() const
{
    if (!m_rememberLastDir) {
        return m_defaultImagesRoot;
    }
    
    QString savedDir = m_settings->value("lastFixedImageDir", m_defaultImagesRoot).toString();
    if (QDir(savedDir).exists()) {
        return savedDir;
    }
    return m_defaultImagesRoot;
}

void AppConfig::setLastFixedImageDir(const QString &dir)
{
    if (m_rememberLastDir) {
        m_settings->setValue("lastFixedImageDir", dir);
    }
}

QString AppConfig::lastMovingImageDir() const
{
    if (!m_rememberLastDir) {
        return m_defaultImagesRoot;
    }
    
    QString savedDir = m_settings->value("lastMovingImageDir", m_defaultImagesRoot).toString();
    if (QDir(savedDir).exists()) {
        return savedDir;
    }
    return m_defaultImagesRoot;
}

void AppConfig::setLastMovingImageDir(const QString &dir)
{
    if (m_rememberLastDir) {
        m_settings->setValue("lastMovingImageDir", dir);
    }
}

QString AppConfig::lastGTExportDir() const
{
    if (!m_rememberLastDir) {
        return m_defaultLabelsRoot;
    }
    
    QString savedDir = m_settings->value("lastGTExportDir", m_defaultLabelsRoot).toString();
    if (QDir(savedDir).exists()) {
        return savedDir;
    }
    return m_defaultLabelsRoot;
}

void AppConfig::setLastGTExportDir(const QString &dir)
{
    if (m_rememberLastDir) {
        m_settings->setValue("lastGTExportDir", dir);
    }
}

// ============================================================================
// Project Cache
// ============================================================================

void AppConfig::saveProjectState(const QString &fixedImageDir,
                                  int fixedIndex, int movingIndex,
                                  const QString &movingImageDir,
                                  const QString &matrixExportDir,
                                  const QString &tiePointsExportDir)
{
    if (!m_rememberLastDir || fixedImageDir.isEmpty())
        return;
    
    // Use fixed image directory as project key
    // Create a unique key based on path (replace special characters)
    QString projectKey = fixedImageDir;
    projectKey.replace("/", "_").replace("\\", "_").replace(":", "_");
    
    m_settings->beginGroup("Projects/" + projectKey);
    m_settings->setValue("fixedImageDir", fixedImageDir);
    m_settings->setValue("fixedIndex", fixedIndex);
    m_settings->setValue("movingIndex", movingIndex);
    m_settings->setValue("movingImageDir", movingImageDir);
    m_settings->setValue("matrixExportDir", matrixExportDir);
    m_settings->setValue("tiePointsExportDir", tiePointsExportDir);
    m_settings->endGroup();
    
    // Also save as last project
    setLastProjectDir(fixedImageDir);
}

bool AppConfig::loadProjectState(const QString &fixedImageDir,
                                  int &fixedIndex, int &movingIndex,
                                  QString &movingImageDir,
                                  QString &matrixExportDir,
                                  QString &tiePointsExportDir)
{
    if (!m_rememberLastDir || fixedImageDir.isEmpty())
        return false;
    
    QString projectKey = fixedImageDir;
    projectKey.replace("/", "_").replace("\\", "_").replace(":", "_");
    
    m_settings->beginGroup("Projects/" + projectKey);
    
    // Check if this project exists
    if (!m_settings->contains("fixedImageDir")) {
        m_settings->endGroup();
        return false;
    }
    
    fixedIndex = m_settings->value("fixedIndex", 0).toInt();
    movingIndex = m_settings->value("movingIndex", 0).toInt();
    movingImageDir = m_settings->value("movingImageDir", "").toString();
    matrixExportDir = m_settings->value("matrixExportDir", "").toString();
    tiePointsExportDir = m_settings->value("tiePointsExportDir", "").toString();
    
    m_settings->endGroup();
    return true;
}

QString AppConfig::lastProjectDir() const
{
    if (!m_rememberLastDir)
        return QString();
    
    QString dir = m_settings->value("lastProjectDir", "").toString();
    if (!dir.isEmpty() && QDir(dir).exists())
        return dir;
    return QString();
}

void AppConfig::setLastProjectDir(const QString &dir)
{
    if (m_rememberLastDir) {
        m_settings->setValue("lastProjectDir", dir);
    }
}

// ============================================================================
// UI Options State
// ============================================================================

bool AppConfig::optionOriginTopLeft() const
{
    return m_settings->value("options/originTopLeft", false).toBool();
}

void AppConfig::setOptionOriginTopLeft(bool value)
{
    m_settings->setValue("options/originTopLeft", value);
}

bool AppConfig::optionNormalizedMatrix() const
{
    return m_settings->value("options/normalizedMatrix", true).toBool();
}

void AppConfig::setOptionNormalizedMatrix(bool value)
{
    m_settings->setValue("options/normalizedMatrix", value);
}

bool AppConfig::optionShowPointLabels() const
{
    return m_settings->value("options/showPointLabels", true).toBool();
}

void AppConfig::setOptionShowPointLabels(bool value)
{
    m_settings->setValue("options/showPointLabels", value);
}

bool AppConfig::optionSyncZoom() const
{
    return m_settings->value("options/syncZoom", true).toBool();
}

void AppConfig::setOptionSyncZoom(bool value)
{
    m_settings->setValue("options/syncZoom", value);
}

int AppConfig::optionTransformMode() const
{
    return m_settings->value("options/transformMode", 0).toInt();
}

void AppConfig::setOptionTransformMode(int mode)
{
    m_settings->setValue("options/transformMode", mode);
}

QString AppConfig::optionLanguage() const
{
    return m_settings->value("options/language", "zh").toString();
}

void AppConfig::setOptionLanguage(const QString &lang)
{
    m_settings->setValue("options/language", lang);
}

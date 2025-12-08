#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>
#include <QSettings>

/**
 * @brief Application configuration manager.
 * 
 * Reads configuration from app.yaml and provides access to settings.
 */
class AppConfig
{
public:
    static AppConfig& instance();

    // Load configuration from file
    bool load(const QString &configPath = QString());

    // Backend settings
    QString backendBaseUrl() const { return m_backendBaseUrl; }

    // Path settings
    QString defaultImagesRoot() const { return m_defaultImagesRoot; }
    QString defaultLabelsRoot() const { return m_defaultLabelsRoot; }

    // UI settings
    QString language() const { return m_language; }
    QString theme() const { return m_theme; }
    bool linkViewsByDefault() const { return m_linkViewsByDefault; }
    bool rememberLastDir() const { return m_rememberLastDir; }

    // Transform settings
    bool allowScaleDefault() const { return m_allowScaleDefault; }
    int minPointsRequired() const { return m_minPointsRequired; }

    // Persistent settings (saved between sessions)
    QString lastFixedImageDir() const;
    void setLastFixedImageDir(const QString &dir);
    QString lastMovingImageDir() const;
    void setLastMovingImageDir(const QString &dir);
    
    // GT export settings
    QString lastGTExportDir() const;
    void setLastGTExportDir(const QString &dir);
    
    // Project cache (keyed by fixed image directory)
    // Saves and restores working state for each project
    void saveProjectState(const QString &fixedImageDir, 
                          int fixedIndex, int movingIndex,
                          const QString &movingImageDir,
                          const QString &matrixExportDir,
                          const QString &tiePointsExportDir);
    bool loadProjectState(const QString &fixedImageDir,
                          int &fixedIndex, int &movingIndex,
                          QString &movingImageDir,
                          QString &matrixExportDir,
                          QString &tiePointsExportDir);
    
    // Last opened project
    QString lastProjectDir() const;
    void setLastProjectDir(const QString &dir);
    
    // UI Options state (persistent across sessions)
    bool optionOriginTopLeft() const;
    void setOptionOriginTopLeft(bool value);
    bool optionNormalizedMatrix() const;
    void setOptionNormalizedMatrix(bool value);
    bool optionShowPointLabels() const;
    void setOptionShowPointLabels(bool value);
    bool optionSyncZoom() const;
    void setOptionSyncZoom(bool value);
    int optionTransformMode() const;
    void setOptionTransformMode(int mode);
    QString optionLanguage() const;
    void setOptionLanguage(const QString &lang);

private:
    AppConfig();
    ~AppConfig() = default;
    AppConfig(const AppConfig&) = delete;
    AppConfig& operator=(const AppConfig&) = delete;

    // Backend
    QString m_backendBaseUrl;

    // Paths
    QString m_defaultImagesRoot;
    QString m_defaultLabelsRoot;

    // UI
    QString m_language;
    QString m_theme;
    bool m_linkViewsByDefault;
    bool m_rememberLastDir;

    // Transform
    bool m_allowScaleDefault;
    int m_minPointsRequired;

    // Settings storage
    QSettings *m_settings;
};

#endif // APPCONFIG_H

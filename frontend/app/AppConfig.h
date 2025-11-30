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

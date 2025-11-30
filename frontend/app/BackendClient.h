#ifndef BACKENDCLIENT_H
#define BACKENDCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointF>
#include <functional>

struct TiePoint;

/**
 * @brief Rigid transformation parameters.
 */
struct RigidParams {
    double theta_deg = 0.0;  // Rotation angle in degrees
    double tx = 0.0;         // Translation X
    double ty = 0.0;         // Translation Y
    double scale = 1.0;      // Uniform scale
};

/**
 * @brief Result of rigid transformation computation.
 */
struct ComputeRigidResult {
    bool success = false;
    QString errorMessage;
    QString errorCode;
    
    RigidParams rigid;
    QVector<QVector<double>> matrix3x3;
    double rmsError = 0.0;
    int numPoints = 0;
};

/**
 * @brief Result of saving a label.
 */
struct LabelSaveResult {
    bool success = false;
    QString errorMessage;
    QString errorCode;
    
    QString labelPath;
    QString labelId;
};

/**
 * @brief Complete label data.
 */
struct LabelData {
    bool success = false;
    QString errorMessage;
    QString errorCode;
    
    QString imageFixed;
    QString imageMoving;
    RigidParams rigid;
    QVector<QVector<double>> matrix3x3;
    QList<QPair<QPointF, QPointF>> tiePoints;
    QString comment;
    QString timestamp;
};

/**
 * @brief Health check result.
 */
struct HealthCheckResult {
    bool success = false;
    QString errorMessage;
    QString version;
    QString backend;
};

/**
 * @brief Result of checkerboard preview request.
 */
struct CheckerboardPreviewResult {
    bool success = false;
    QString errorMessage;
    QString errorCode;
    
    QString imageBase64;  // Base64-encoded PNG image
    int width = 0;
    int height = 0;
};

/**
 * @brief Client for communicating with the FastAPI backend.
 * 
 * Handles HTTP requests to the backend server for:
 * - Health check
 * - Rigid transformation computation
 * - Label save/load operations
 */
class BackendClient : public QObject
{
    Q_OBJECT

public:
    explicit BackendClient(QObject *parent = nullptr);
    explicit BackendClient(const QString &baseUrl, QObject *parent = nullptr);

    void setBaseUrl(const QString &url);
    QString baseUrl() const { return m_baseUrl; }

    // API methods
    void healthCheck();
    void computeRigid(const QList<QPair<QPointF, QPointF>> &tiePoints, 
                      bool allowScale = false,
                      int minPointsRequired = 2);
    void saveLabel(const QString &imageFixed,
                   const QString &imageMoving,
                   const RigidParams &rigid,
                   const QVector<QVector<double>> &matrix3x3,
                   const QList<QPair<QPointF, QPointF>> &tiePoints,
                   const QString &comment = QString());
    void loadLabel(const QString &imageFixed, const QString &imageMoving);
    void listLabels();
    void requestCheckerboardPreview(const QString &imageFixed,
                                    const QString &imageMoving,
                                    const QVector<QVector<double>> &matrix3x3,
                                    int boardSize = 8,
                                    bool useCenterOrigin = false);

signals:
    void healthCheckCompleted(const HealthCheckResult &result);
    void computeRigidCompleted(const ComputeRigidResult &result);
    void saveLabelCompleted(const LabelSaveResult &result);
    void loadLabelCompleted(const LabelData &result);
    void listLabelsCompleted(bool success, const QList<QJsonObject> &labels, const QString &error);
    void checkerboardPreviewCompleted(const CheckerboardPreviewResult &result);
    void networkError(const QString &message);

private slots:
    void handleHealthReply();
    void handleComputeRigidReply();
    void handleSaveLabelReply();
    void handleLoadLabelReply();
    void handleListLabelsReply();
    void handleCheckerboardPreviewReply();

private:
    QNetworkRequest createRequest(const QString &endpoint) const;
    QJsonObject parseResponse(QNetworkReply *reply, bool &ok, QString &errorMsg);
    
    QNetworkAccessManager *m_networkManager;
    QString m_baseUrl;
};

#endif // BACKENDCLIENT_H

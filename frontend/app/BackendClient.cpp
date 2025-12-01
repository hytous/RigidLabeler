#include "BackendClient.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDebug>

BackendClient::BackendClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_baseUrl("http://127.0.0.1:8000")
{
}

BackendClient::BackendClient(const QString &baseUrl, QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_baseUrl(baseUrl)
{
}

void BackendClient::setBaseUrl(const QString &url)
{
    m_baseUrl = url;
}

QNetworkRequest BackendClient::createRequest(const QString &endpoint) const
{
    QUrl url(m_baseUrl + endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    return request;
}

QJsonObject BackendClient::parseResponse(QNetworkReply *reply, bool &ok, QString &errorMsg)
{
    ok = false;
    
    if (reply->error() != QNetworkReply::NoError) {
        errorMsg = reply->errorString();
        return QJsonObject();
    }
    
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        errorMsg = "Invalid JSON response";
        return QJsonObject();
    }
    
    ok = true;
    return doc.object();
}

// ============================================================================
// Health Check
// ============================================================================

void BackendClient::healthCheck()
{
    QNetworkRequest request = createRequest("/health");
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &BackendClient::handleHealthReply);
}

void BackendClient::handleHealthReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    
    HealthCheckResult result;
    bool ok;
    QString errorMsg;
    
    QJsonObject json = parseResponse(reply, ok, errorMsg);
    
    if (!ok) {
        result.errorMessage = errorMsg;
        emit healthCheckCompleted(result);
        return;
    }
    
    QString status = json["status"].toString();
    if (status != "ok") {
        result.errorMessage = json["message"].toString();
        emit healthCheckCompleted(result);
        return;
    }
    
    QJsonObject data = json["data"].toObject();
    result.success = true;
    result.version = data["version"].toString();
    result.backend = data["backend"].toString();
    
    emit healthCheckCompleted(result);
}

// ============================================================================
// Compute Rigid
// ============================================================================

void BackendClient::computeRigid(const QList<QPair<QPointF, QPointF>> &tiePoints,
                                  const QString &transformMode,
                                  int minPointsRequired)
{
    QJsonArray pointsArray;
    for (const auto &pair : tiePoints) {
        QJsonObject tpObj;
        tpObj["fixed"] = QJsonObject{{"x", pair.first.x()}, {"y", pair.first.y()}};
        tpObj["moving"] = QJsonObject{{"x", pair.second.x()}, {"y", pair.second.y()}};
        pointsArray.append(tpObj);
    }
    
    QJsonObject requestBody;
    requestBody["tie_points"] = pointsArray;
    requestBody["transform_mode"] = transformMode;
    requestBody["min_points_required"] = minPointsRequired;
    
    QNetworkRequest request = createRequest("/compute/rigid");
    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(requestBody).toJson());
    connect(reply, &QNetworkReply::finished, this, &BackendClient::handleComputeRigidReply);
}

void BackendClient::handleComputeRigidReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    
    ComputeRigidResult result;
    bool ok;
    QString errorMsg;
    
    QJsonObject json = parseResponse(reply, ok, errorMsg);
    
    if (!ok) {
        result.errorMessage = errorMsg;
        emit computeRigidCompleted(result);
        return;
    }
    
    QString status = json["status"].toString();
    if (status != "ok") {
        result.errorMessage = json["message"].toString();
        result.errorCode = json["error_code"].toString();
        emit computeRigidCompleted(result);
        return;
    }
    
    QJsonObject data = json["data"].toObject();
    QJsonObject rigid = data["rigid"].toObject();
    
    result.success = true;
    result.rigid.theta_deg = rigid["theta_deg"].toDouble();
    result.rigid.tx = rigid["tx"].toDouble();
    result.rigid.ty = rigid["ty"].toDouble();
    result.rigid.scale_x = rigid["scale_x"].toDouble(1.0);
    result.rigid.scale_y = rigid["scale_y"].toDouble(1.0);
    result.rigid.shear = rigid["shear"].toDouble(0.0);
    result.rmsError = data["rms_error"].toDouble();
    result.numPoints = data["num_points"].toInt();
    
    // Parse matrix
    QJsonArray matrixArray = data["matrix_3x3"].toArray();
    result.matrix3x3.resize(3);
    for (int i = 0; i < 3; ++i) {
        QJsonArray row = matrixArray[i].toArray();
        result.matrix3x3[i].resize(3);
        for (int j = 0; j < 3; ++j) {
            result.matrix3x3[i][j] = row[j].toDouble();
        }
    }
    
    emit computeRigidCompleted(result);
}

// ============================================================================
// Save Label
// ============================================================================

void BackendClient::saveLabel(const QString &imageFixed,
                               const QString &imageMoving,
                               const RigidParams &rigid,
                               const QVector<QVector<double>> &matrix3x3,
                               const QList<QPair<QPointF, QPointF>> &tiePoints,
                               const QString &comment)
{
    QJsonObject rigidObj;
    rigidObj["theta_deg"] = rigid.theta_deg;
    rigidObj["tx"] = rigid.tx;
    rigidObj["ty"] = rigid.ty;
    rigidObj["scale_x"] = rigid.scale_x;
    rigidObj["scale_y"] = rigid.scale_y;
    rigidObj["shear"] = rigid.shear;
    
    QJsonArray matrixArray;
    for (const auto &row : matrix3x3) {
        QJsonArray rowArray;
        for (double val : row) {
            rowArray.append(val);
        }
        matrixArray.append(rowArray);
    }
    
    QJsonArray pointsArray;
    for (const auto &pair : tiePoints) {
        QJsonObject tpObj;
        tpObj["fixed"] = QJsonObject{{"x", pair.first.x()}, {"y", pair.first.y()}};
        tpObj["moving"] = QJsonObject{{"x", pair.second.x()}, {"y", pair.second.y()}};
        pointsArray.append(tpObj);
    }
    
    QJsonObject requestBody;
    requestBody["image_fixed"] = imageFixed;
    requestBody["image_moving"] = imageMoving;
    requestBody["rigid"] = rigidObj;
    requestBody["matrix_3x3"] = matrixArray;
    requestBody["tie_points"] = pointsArray;
    
    if (!comment.isEmpty()) {
        QJsonObject meta;
        meta["comment"] = comment;
        requestBody["meta"] = meta;
    }
    
    QNetworkRequest request = createRequest("/labels/save");
    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(requestBody).toJson());
    connect(reply, &QNetworkReply::finished, this, &BackendClient::handleSaveLabelReply);
}

void BackendClient::handleSaveLabelReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    
    LabelSaveResult result;
    bool ok;
    QString errorMsg;
    
    QJsonObject json = parseResponse(reply, ok, errorMsg);
    
    if (!ok) {
        result.errorMessage = errorMsg;
        emit saveLabelCompleted(result);
        return;
    }
    
    QString status = json["status"].toString();
    if (status != "ok") {
        result.errorMessage = json["message"].toString();
        result.errorCode = json["error_code"].toString();
        emit saveLabelCompleted(result);
        return;
    }
    
    QJsonObject data = json["data"].toObject();
    result.success = true;
    result.labelPath = data["label_path"].toString();
    result.labelId = data["label_id"].toString();
    
    emit saveLabelCompleted(result);
}

// ============================================================================
// Load Label
// ============================================================================

void BackendClient::loadLabel(const QString &imageFixed, const QString &imageMoving)
{
    QUrl url(m_baseUrl + "/labels/load");
    QUrlQuery query;
    query.addQueryItem("image_fixed", imageFixed);
    query.addQueryItem("image_moving", imageMoving);
    url.setQuery(query);
    
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &BackendClient::handleLoadLabelReply);
}

void BackendClient::handleLoadLabelReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    
    LabelData result;
    bool ok;
    QString errorMsg;
    
    QJsonObject json = parseResponse(reply, ok, errorMsg);
    
    if (!ok) {
        result.errorMessage = errorMsg;
        emit loadLabelCompleted(result);
        return;
    }
    
    QString status = json["status"].toString();
    if (status != "ok") {
        result.errorMessage = json["message"].toString();
        result.errorCode = json["error_code"].toString();
        emit loadLabelCompleted(result);
        return;
    }
    
    QJsonObject data = json["data"].toObject();
    
    result.success = true;
    result.imageFixed = data["image_fixed"].toString();
    result.imageMoving = data["image_moving"].toString();
    
    QJsonObject rigid = data["rigid"].toObject();
    result.rigid.theta_deg = rigid["theta_deg"].toDouble();
    result.rigid.tx = rigid["tx"].toDouble();
    result.rigid.ty = rigid["ty"].toDouble();
    result.rigid.scale_x = rigid["scale_x"].toDouble(1.0);
    result.rigid.scale_y = rigid["scale_y"].toDouble(1.0);
    result.rigid.shear = rigid["shear"].toDouble(0.0);
    
    // Parse matrix
    QJsonArray matrixArray = data["matrix_3x3"].toArray();
    result.matrix3x3.resize(3);
    for (int i = 0; i < 3; ++i) {
        QJsonArray row = matrixArray[i].toArray();
        result.matrix3x3[i].resize(3);
        for (int j = 0; j < 3; ++j) {
            result.matrix3x3[i][j] = row[j].toDouble();
        }
    }
    
    // Parse tie points
    QJsonArray pointsArray = data["tie_points"].toArray();
    for (const QJsonValue &val : pointsArray) {
        QJsonObject tpObj = val.toObject();
        QJsonObject fixed = tpObj["fixed"].toObject();
        QJsonObject moving = tpObj["moving"].toObject();
        result.tiePoints.append({
            QPointF(fixed["x"].toDouble(), fixed["y"].toDouble()),
            QPointF(moving["x"].toDouble(), moving["y"].toDouble())
        });
    }
    
    // Parse meta
    if (data.contains("meta") && !data["meta"].isNull()) {
        QJsonObject meta = data["meta"].toObject();
        result.comment = meta["comment"].toString();
        result.timestamp = meta["timestamp"].toString();
    }
    
    emit loadLabelCompleted(result);
}

// ============================================================================
// List Labels
// ============================================================================

void BackendClient::listLabels()
{
    QNetworkRequest request = createRequest("/labels/list");
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &BackendClient::handleListLabelsReply);
}

void BackendClient::handleListLabelsReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    
    bool ok;
    QString errorMsg;
    QJsonObject json = parseResponse(reply, ok, errorMsg);
    
    if (!ok) {
        emit listLabelsCompleted(false, {}, errorMsg);
        return;
    }
    
    QString status = json["status"].toString();
    if (status != "ok") {
        emit listLabelsCompleted(false, {}, json["message"].toString());
        return;
    }
    
    QList<QJsonObject> labels;
    QJsonArray dataArray = json["data"].toArray();
    for (const QJsonValue &val : dataArray) {
        labels.append(val.toObject());
    }
    
    emit listLabelsCompleted(true, labels, QString());
}

// ============================================================================
// Checkerboard Preview
// ============================================================================

void BackendClient::requestCheckerboardPreview(const QString &imageFixed,
                                                const QString &imageMoving,
                                                const QVector<QVector<double>> &matrix3x3,
                                                int boardSize,
                                                bool useCenterOrigin)
{
    QJsonArray matrixArray;
    for (const auto &row : matrix3x3) {
        QJsonArray rowArray;
        for (double val : row) {
            rowArray.append(val);
        }
        matrixArray.append(rowArray);
    }
    
    QJsonObject requestBody;
    requestBody["image_fixed"] = imageFixed;
    requestBody["image_moving"] = imageMoving;
    requestBody["matrix_3x3"] = matrixArray;
    requestBody["board_size"] = boardSize;
    requestBody["use_center_origin"] = useCenterOrigin;
    
    QNetworkRequest request = createRequest("/warp/checkerboard");
    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(requestBody).toJson());
    connect(reply, &QNetworkReply::finished, this, &BackendClient::handleCheckerboardPreviewReply);
}

void BackendClient::handleCheckerboardPreviewReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    
    CheckerboardPreviewResult result;
    bool ok;
    QString errorMsg;
    
    QJsonObject json = parseResponse(reply, ok, errorMsg);
    
    if (!ok) {
        result.errorMessage = errorMsg;
        emit checkerboardPreviewCompleted(result);
        return;
    }
    
    QString status = json["status"].toString();
    if (status != "ok") {
        result.errorMessage = json["message"].toString();
        result.errorCode = json["error_code"].toString();
        emit checkerboardPreviewCompleted(result);
        return;
    }
    
    QJsonObject data = json["data"].toObject();
    result.success = true;
    result.imageBase64 = data["image_base64"].toString();
    result.width = data["width"].toInt();
    result.height = data["height"].toInt();
    
    emit checkerboardPreviewCompleted(result);
}

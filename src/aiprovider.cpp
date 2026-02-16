#include "AiProvider.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

AiProvider::AiProvider(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_serviceType(OpenAI)
    , m_apiUrl("https://api.openai.com/v1/chat/completions")
    , m_modelsApiUrl("https://api.openai.com/v1/models")
    , m_model("gpt-3.5-turbo")
{
}

AiProvider::~AiProvider()
{
}

void AiProvider::setServiceType(ServiceType type)
{
    m_serviceType = type;

    switch (type) {
    case OpenAI:
        m_apiUrl = "https://api.openai.com/v1/chat/completions";
        m_modelsApiUrl = "https://api.openai.com/v1/models";
        m_model = "gpt-3.5-turbo";
        break;

    case DeepSeek:
        m_apiUrl = "https://api.deepseek.com/v1/chat/completions";
        m_modelsApiUrl = "https://api.deepseek.com/v1/models";
        m_model = "deepseek-chat";
        break;

    case Custom:
        // 自定义需要手动设置
        break;
    }
}

void AiProvider::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey;
}

void AiProvider::setApiUrl(const QString &url)
{
    m_apiUrl = url;
}

void AiProvider::setModel(const QString &model)
{
    m_model = model;
}

// ========== 从 API 获取模型列表 ==========
void AiProvider::fetchModels()
{
    if (m_apiKey.isEmpty()) {
        emit errorOccurred("❌ API Key 未设置");
        return;
    }

    if (m_modelsApiUrl.isEmpty()) {
        emit errorOccurred("❌ 该服务未配置模型列表 API");
        return;
    }

    QNetworkRequest request(m_modelsApiUrl);

    // 根据服务类型设置请求头
    request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    qDebug() << "=== Fetching Models ===";
    qDebug() << "URL:" << m_modelsApiUrl;

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, &AiProvider::handleModelsReply);
}

void AiProvider::handleModelsReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QByteArray data = reply->readAll();

    qDebug() << "=== Models Response ===";
    qDebug() << "Status Code:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "Data:" << data;

    // 检查网络错误
    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = QString("获取模型列表失败 [%1]: %2")
                               .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
                               .arg(reply->errorString());

        // 尝试解析错误详情
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject() && doc.object().contains("error")) {
            QJsonObject errorObj = doc.object()["error"].toObject();
            QString apiError = errorObj["message"].toString();
            if (!apiError.isEmpty()) {
                errorMsg += "\nAPI 错误：" + apiError;
            }
        }

        emit errorOccurred("❌ " + errorMsg);
        reply->deleteLater();
        return;
    }

    // 解析 JSON
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit errorOccurred("❌ 无效的 JSON 响应格式");
        reply->deleteLater();
        return;
    }

    QJsonObject obj = doc.object();
    QList<ModelInfo> models;

    // 解析模型列表
    if (obj.contains("data")) {
        QJsonArray dataArray = obj["data"].toArray();

        qDebug() << "Found" << dataArray.size() << "models";

        for (const QJsonValue &value : dataArray) {
            QJsonObject modelObj = value.toObject();

            ModelInfo info;
            info.id = modelObj["id"].toString();
            info.ownedBy = modelObj["owned_by"].toString();

            // 获取创建时间
            if (modelObj.contains("created")) {
                qint64 timestamp = modelObj["created"].toInteger();
                info.created = QDateTime::fromSecsSinceEpoch(timestamp).toString("yyyy-MM-dd");
            }

            // 获取权限信息（如果有）
            if (modelObj.contains("permission")) {
                QJsonArray permArray = modelObj["permission"].toArray();
                for (const QJsonValue &perm : permArray) {
                    info.permissions.append(perm.toString());
                }
            }

            models.append(info);
            qDebug() << "  -" << info.id << "(" << info.ownedBy << ")";
        }
    } else {
        emit errorOccurred("❌ 响应格式错误：缺少 'data' 字段");
        reply->deleteLater();
        return;
    }

    if (models.isEmpty()) {
        emit errorOccurred("⚠️ 未获取到任何模型");
    } else {
        emit modelsReceived(models);
    }

    reply->deleteLater();
}

// ========== 聊天功能 ==========
void AiProvider::chat(const QString &message)
{
    if (m_apiKey.isEmpty()) {
        emit errorOccurred("❌ API Key 未设置");
        return;
    }

    if (m_apiUrl.isEmpty()) {
        emit errorOccurred("❌ API URL 未设置");
        return;
    }

    QNetworkRequest request(m_apiUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    QJsonObject json;
    json["model"] = m_model;

    QJsonArray messages;

    // 如果系统提示词不为空，先添加系统消息
    if (!m_systemPrompt.isEmpty()) {
        QJsonObject systemMsg;
        systemMsg["role"] = "system";
        systemMsg["content"] = m_systemPrompt;
        messages.append(systemMsg);
    }

    // 添加用户消息
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = message;
    messages.append(userMsg);

    json["messages"] = messages;

    qDebug() << "=== AI Request ===";
    qDebug() << "URL:" << m_apiUrl;
    qDebug() << "Model:" << m_model;
    qDebug() << "Message:" << message;

    QNetworkReply *reply = m_network->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, &AiProvider::handleReply);
}


void AiProvider::handleReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QByteArray data = reply->readAll();

    qDebug() << "=== AI Response ===";
    qDebug() << "Status Code:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = QString("网络错误 [%1]: %2")
                               .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
                               .arg(reply->errorString());

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject() && doc.object().contains("error")) {
            QJsonObject errorObj = doc.object()["error"].toObject();
            QString apiError = errorObj["message"].toString();
            if (!apiError.isEmpty()) {
                errorMsg += "\nAPI 错误：" + apiError;
            }
        }

        emit errorOccurred("❌ " + errorMsg);
        reply->deleteLater();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit errorOccurred("❌ 无效的响应格式");
        reply->deleteLater();
        return;
    }

    QJsonObject obj = doc.object();

    if (obj.contains("error")) {
        QString errorMsg = obj["error"].toObject()["message"].toString();
        emit errorOccurred("❌ API 错误：" + errorMsg);
        reply->deleteLater();
        return;
    }

    if (obj.contains("choices")) {
        QJsonArray choices = obj["choices"].toArray();
        if (!choices.isEmpty()) {
            QString content = choices[0].toObject()["message"].toObject()["content"].toString();
            emit replyReceived(content);
        } else {
            emit errorOccurred("❌ 响应中没有内容");
        }
    } else {
        emit errorOccurred("❌ 响应格式错误：缺少 choices 字段");
    }

    reply->deleteLater();
}


void AiProvider::setSystemPrompt(const QString &prompt)
{
    m_systemPrompt = prompt;
}

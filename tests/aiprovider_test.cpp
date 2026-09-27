#include <AiProvider.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QTimer>
#include <memory>

class HttpFixture : public QTcpServer
{
public:
    QByteArray request;
    QList<QByteArray> chunks;
    int status{200};
    QByteArray contentType{"application/json"};
    HttpFixture()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto *socket = nextPendingConnection();
            auto received = std::make_shared<QByteArray>();
            auto replied = std::make_shared<bool>(false);
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [=] {
                received->append(socket->readAll());
                const auto headerEnd = received->indexOf("\r\n\r\n");
                if (*replied || headerEnd < 0)
                    return;
                int bodySize = 0;
                for (auto line : received->left(headerEnd).split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        bodySize = line.mid(15).trimmed().toInt();
                if (received->size() < headerEnd + 4 + bodySize)
                    return;
                *replied = true;
                request = *received;
                qsizetype size = 0;
                for (const auto &chunk : chunks)
                    size += chunk.size();
                socket->write("HTTP/1.1 " + QByteArray::number(status) + " Fixture\r\nContent-Type: " +
                    contentType + "\r\nContent-Length: " + QByteArray::number(size) + "\r\nConnection: close\r\n\r\n");
                for (qsizetype i = 0; i < chunks.size(); ++i)
                    QTimer::singleShot(int((i + 1) * 10), socket, [=] {
                        socket->write(chunks[i]);
                        if (i + 1 == chunks.size())
                            socket->disconnectFromHost();
                    });
            });
        });
        if (!listen(QHostAddress::LocalHost))
            qFatal("Cannot create local HTTP fixture");
    }
    QString url() const { return QStringLiteral("http://127.0.0.1:%1/v1").arg(serverPort()); }
    QByteArray header(const QByteArray &name) const
    {
        for (const auto &line : request.left(request.indexOf("\r\n\r\n")).split('\n'))
        {
            const auto colon = line.indexOf(':');
            if (colon > 0 && line.left(colon).compare(name, Qt::CaseInsensitive) == 0)
                return line.mid(colon + 1).trimmed();
        }
        return {};
    }
    void configure(AiProvider &provider)
    {
        provider.setServiceType(AiProvider::Custom);
        provider.setBaseUrl(url());
        provider.setApiKey(QStringLiteral("fixture-key"));
        provider.setModel(QStringLiteral("fixture-model"));
    }
};

class AiProviderTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        qRegisterMetaType<QList<AiProvider::ModelInfo>>();
    }
    void tlsBackendAvailable() { QVERIFY(QSslSocket::supportsSsl()); }
    void serviceDefaults()
    {
        const QHash<AiProvider::ServiceType, QString> expectedModels{
            {AiProvider::OpenAI, "gpt-5-mini"},
            {AiProvider::DeepSeek, "deepseek-chat"},
            {AiProvider::Qwen, "qwen-plus"},
            {AiProvider::Moonshot, "kimi-k2.6"},
            {AiProvider::Zhipu, "glm-4.7"},
            {AiProvider::Doubao, "doubao-seed-evolving"},
            {AiProvider::SiliconFlow, "deepseek-ai/DeepSeek-V4-Flash"},
            {AiProvider::Anthropic, "claude-sonnet-4-5"},
            {AiProvider::Gemini, "gemini-3.8-flash"},
            {AiProvider::Grok, "grok-4"}};
        for (auto it = expectedModels.constBegin(); it != expectedModels.constEnd(); ++it)
        {
            AiProvider provider;
            provider.setServiceType(it.key());
            QCOMPARE(provider.currentServiceType(), it.key());
            QCOMPARE(provider.currentModel(), it.value());
        }
    }
    void modelList()
    {
        HttpFixture server;
        server.chunks = {R"({"data":[{"id":"fixture-model","owned_by":"fixture","created":1700000000}]})"};
        AiProvider provider;
        server.configure(provider);
        QList<AiProvider::ModelInfo> models;
        connect(&provider, &AiProvider::modelsReceived, this, [&](const auto &value) { models = value; });
        QSignalSpy errors(&provider, &AiProvider::errorOccurred);
        provider.fetchModels();
        QTRY_COMPARE_WITH_TIMEOUT(models.size(), 1, 5000);
        QCOMPARE(models.first().id, QStringLiteral("fixture-model"));
        QCOMPARE(models.first().ownedBy, QStringLiteral("fixture"));
        QVERIFY(server.request.startsWith("GET /v1/models "));
        QCOMPARE(server.header("authorization"), QByteArray("Bearer fixture-key"));
        QCOMPARE(errors.size(), 0);
    }
    void chatAndSystemPrompt()
    {
        HttpFixture server;
        server.chunks = {R"({"choices":[{"message":{"content":"fixture reply"}}]})"};
        AiProvider provider;
        server.configure(provider);
        provider.setSystemPrompt(QStringLiteral("system instruction"));
        QSignalSpy replies(&provider, &AiProvider::replyReceived);
        QSignalSpy errors(&provider, &AiProvider::errorOccurred);
        provider.chat(QStringLiteral("hello"));
        QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
        QCOMPARE(replies.first().first().toString(), QStringLiteral("fixture reply"));
        QVERIFY(server.request.startsWith("POST /v1/chat/completions "));
        const auto json = QJsonDocument::fromJson(server.request.mid(server.request.indexOf("\r\n\r\n") + 4)).object();
        QCOMPARE(json["model"].toString(), QStringLiteral("fixture-model"));
        QCOMPARE(json["stream"].toBool(), false);
        const auto messages = json["messages"].toArray();
        QCOMPARE(messages.size(), 2);
        QCOMPARE(messages[0].toObject()["role"].toString(), QStringLiteral("system"));
        QCOMPARE(messages[1].toObject()["content"].toString(), QStringLiteral("hello"));
        QCOMPARE(errors.size(), 0);
    }
    void fragmentedStream()
    {
        HttpFixture server;
        server.contentType = "text/event-stream";
        const auto data = QStringLiteral("data: {\"choices\":[{\"delta\":{\"content\":\"你好\"}}]}\r\n\r\ndata: {\"choices\":[{\"delta\":{\"content\":\" Qt\"}}]}\n\ndata: [DONE]\n\n").toUtf8();
        const auto split = data.indexOf(QStringLiteral("好").toUtf8()) + 1;
        server.chunks = {data.left(7), data.mid(7, split - 7), data.mid(split)};
        AiProvider provider;
        server.configure(provider);
        provider.setStreamEnabled(true);
        QSignalSpy chunks(&provider, &AiProvider::replyChunkReceived);
        QSignalSpy replies(&provider, &AiProvider::replyReceived);
        QSignalSpy errors(&provider, &AiProvider::errorOccurred);
        provider.chat(QStringLiteral("hello"));
        QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
        QCOMPARE(replies.first().first().toString(), QStringLiteral("你好 Qt"));
        QCOMPARE(chunks.size(), 2);
        QCOMPARE(errors.size(), 0);
    }
    void anthropicChat()
    {
        HttpFixture server;
        server.chunks = {R"({"id":"msg_1","type":"message","role":"assistant","content":[{"type":"text","text":"fixture reply"}]})"};
        AiProvider provider;
        provider.setServiceType(AiProvider::Anthropic);
        provider.setBaseUrl(server.url());
        provider.setApiKey(QStringLiteral("fixture-key"));
        provider.setModel(QStringLiteral("fixture-model"));
        provider.setSystemPrompt(QStringLiteral("system instruction"));
        QSignalSpy replies(&provider, &AiProvider::replyReceived);
        QSignalSpy errors(&provider, &AiProvider::errorOccurred);
        provider.chat(QStringLiteral("hello"));
        QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
        QCOMPARE(replies.first().first().toString(), QStringLiteral("fixture reply"));
        QVERIFY(server.request.startsWith("POST /v1/messages "));
        QCOMPARE(server.header("x-api-key"), QByteArray("fixture-key"));
        QCOMPARE(server.header("anthropic-version"), QByteArray("2023-06-01"));
        QVERIFY(server.header("authorization").isEmpty());
        const auto json = QJsonDocument::fromJson(server.request.mid(server.request.indexOf("\r\n\r\n") + 4)).object();
        QCOMPARE(json["model"].toString(), QStringLiteral("fixture-model"));
        QCOMPARE(json["max_tokens"].toInt(), 4096);
        QCOMPARE(json["system"].toString(), QStringLiteral("system instruction"));
        QVERIFY(!json.contains("stream") || json["stream"].toBool() == false);
        const auto messages = json["messages"].toArray();
        QCOMPARE(messages.size(), 1);
        QCOMPARE(messages[0].toObject()["role"].toString(), QStringLiteral("user"));
        QCOMPARE(messages[0].toObject()["content"].toString(), QStringLiteral("hello"));
        QCOMPARE(errors.size(), 0);
    }
    void anthropicStream()
    {
        HttpFixture server;
        server.contentType = "text/event-stream";
        const auto data = QByteArray("event: content_block_delta\r\ndata: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"你好\"}}\r\n\r\n"
                                     "event: content_block_delta\ndata: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\" Anthropic\"}}\n\n"
                                     "event: message_stop\ndata: {\"type\":\"message_stop\"}\n\n");
        const auto split = data.indexOf(QStringLiteral("好").toUtf8()) + 1;
        server.chunks = {data.left(7), data.mid(7, split - 7), data.mid(split)};
        AiProvider provider;
        provider.setServiceType(AiProvider::Anthropic);
        provider.setBaseUrl(server.url());
        provider.setApiKey(QStringLiteral("fixture-key"));
        provider.setModel(QStringLiteral("fixture-model"));
        provider.setStreamEnabled(true);
        QSignalSpy chunks(&provider, &AiProvider::replyChunkReceived);
        QSignalSpy replies(&provider, &AiProvider::replyReceived);
        QSignalSpy errors(&provider, &AiProvider::errorOccurred);
        provider.chat(QStringLiteral("hello"));
        QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
        QCOMPARE(replies.first().first().toString(), QStringLiteral("你好 Anthropic"));
        QCOMPARE(chunks.size(), 2);
        QCOMPARE(errors.size(), 0);
    }
    void anthropicModels()
    {
        HttpFixture server;
        server.chunks = {R"({"data":[{"type":"model","id":"claude-fixture","display_name":"Fixture"}]})"};
        AiProvider provider;
        provider.setServiceType(AiProvider::Anthropic);
        provider.setBaseUrl(server.url());
        provider.setApiKey(QStringLiteral("fixture-key"));
        provider.setModel(QStringLiteral("fixture-model"));
        QList<AiProvider::ModelInfo> models;
        connect(&provider, &AiProvider::modelsReceived, this, [&](const auto &value) { models = value; });
        QSignalSpy errors(&provider, &AiProvider::errorOccurred);
        provider.fetchModels();
        QTRY_COMPARE_WITH_TIMEOUT(models.size(), 1, 5000);
        QCOMPARE(models.first().id, QStringLiteral("claude-fixture"));
        QVERIFY(server.request.startsWith("GET /v1/models "));
        QCOMPARE(server.header("x-api-key"), QByteArray("fixture-key"));
        QCOMPARE(errors.size(), 0);
    }
    void failures_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<QString>("error");
        QTest::newRow("http") << 429 << QByteArray(R"({"error":{"message":"rate limit fixture"}})") << QStringLiteral("rate limit fixture");
        QTest::newRow("invalid-json") << 200 << QByteArray("not json") << QStringLiteral("Invalid response JSON");
        QTest::newRow("empty-content") << 200 << QByteArray(R"({"choices":[]})") << QStringLiteral("did not contain any content");
    }
    void failures()
    {
        QFETCH(int, status);
        QFETCH(QByteArray, body);
        QFETCH(QString, error);
        HttpFixture server;
        server.status = status;
        server.chunks = {body};
        AiProvider provider;
        server.configure(provider);
        QSignalSpy errors(&provider, &AiProvider::errorOccurred);
        QSignalSpy replies(&provider, &AiProvider::replyReceived);
        provider.chat(QStringLiteral("hello"));
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 5000);
        QVERIFY2(errors.first().first().toString().contains(error), qPrintable(errors.first().first().toString()));
        QCOMPARE(replies.size(), 0);
    }
    void missingKey()
    {
        AiProvider provider;
        QSignalSpy errors(&provider, &AiProvider::errorOccurred);
        provider.chat(QStringLiteral("hello"));
        QCOMPARE(errors.size(), 1);
    }
};
QTEST_GUILESS_MAIN(AiProviderTest)
#include "aiprovider_test.moc"

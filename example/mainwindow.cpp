#include "mainwindow.h"

#include "aiprovider.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    AiProvider *ai = new AiProvider(this);
    ai->setServiceType(AiProvider::DeepSeek);
    ai->setStreamEnabled(true);
    ai->setProperty("streamReplyActive", false);
    ai->setProperty("streamReplyHasChunks", false);

    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);

    QHBoxLayout *apiKeyLayout = new QHBoxLayout;
    QLabel *apiKeyLabel = new QLabel("API Key:");
    QLineEdit *apiKeyInput = new QLineEdit;
    apiKeyInput->setEchoMode(QLineEdit::Password);
    apiKeyInput->setPlaceholderText("Enter API Key");
    QPushButton *setApiKeyBtn = new QPushButton("Set");
    setApiKeyBtn->setMaximumWidth(80);
    apiKeyLayout->addWidget(apiKeyLabel);
    apiKeyLayout->addWidget(apiKeyInput, 1);
    apiKeyLayout->addWidget(setApiKeyBtn);
    layout->addLayout(apiKeyLayout);

    QHBoxLayout *serviceLayout = new QHBoxLayout;
    QLabel *serviceLabel = new QLabel("Service:");
    QComboBox *serviceSelector = new QComboBox;
    serviceSelector->addItem("OpenAI", AiProvider::OpenAI);
    serviceSelector->addItem("DeepSeek", AiProvider::DeepSeek);
    serviceSelector->setCurrentIndex(1);
    QCheckBox *streamCheckBox = new QCheckBox("Stream");
    streamCheckBox->setChecked(true);
    serviceLayout->addWidget(serviceLabel);
    serviceLayout->addWidget(serviceSelector, 1);
    serviceLayout->addWidget(streamCheckBox);
    layout->addLayout(serviceLayout);

    QHBoxLayout *modelLayout = new QHBoxLayout;
    QLabel *modelLabel = new QLabel("Model:");
    QComboBox *modelSelector = new QComboBox;
    modelSelector->setMinimumWidth(300);
    QPushButton *fetchModelsBtn = new QPushButton("Fetch Models");
    modelLayout->addWidget(modelLabel);
    modelLayout->addWidget(modelSelector, 1);
    modelLayout->addWidget(fetchModelsBtn);
    layout->addLayout(modelLayout);

    QHBoxLayout *systemPromptLayout = new QHBoxLayout;
    QLabel *systemPromptLabel = new QLabel("System Prompt:");
    QLineEdit *systemPromptInput = new QLineEdit;
    systemPromptInput->setPlaceholderText("Optional system prompt");
    systemPromptLayout->addWidget(systemPromptLabel);
    systemPromptLayout->addWidget(systemPromptInput, 1);
    layout->addLayout(systemPromptLayout);

    layout->addWidget(new QLabel("Chat:"));
    QTextEdit *chatDisplay = new QTextEdit;
    chatDisplay->setReadOnly(true);
    layout->addWidget(chatDisplay);

    QHBoxLayout *inputLayout = new QHBoxLayout;
    QLineEdit *input = new QLineEdit;
    input->setPlaceholderText("Type message...");
    QPushButton *sendBtn = new QPushButton("Send");
    inputLayout->addWidget(input);
    inputLayout->addWidget(sendBtn);
    layout->addLayout(inputLayout);

    setCentralWidget(central);
    resize(800, 600);
    setWindowTitle("ZcAiLib Example");

    auto setChatBusy = [=](bool busy) {
        sendBtn->setEnabled(!busy);
        input->setEnabled(!busy);
        if (!busy) {
            input->setFocus();
        }
    };

    auto finishStreamUi = [=]() {
        if (!ai->property("streamReplyActive").toBool()) {
            return;
        }

        chatDisplay->moveCursor(QTextCursor::End);
        chatDisplay->insertPlainText("\n\n");
        chatDisplay->ensureCursorVisible();
        ai->setProperty("streamReplyActive", false);
        ai->setProperty("streamReplyHasChunks", false);
    };

    connect(systemPromptInput, &QLineEdit::textChanged, ai, &AiProvider::setSystemPrompt);

    connect(streamCheckBox, &QCheckBox::toggled, [=](bool checked) {
        ai->setStreamEnabled(checked);
        chatDisplay->append(QString("[config] stream %1").arg(checked ? "enabled" : "disabled"));
    });

    connect(setApiKeyBtn, &QPushButton::clicked, [=]() {
        const QString apiKey = apiKeyInput->text().trimmed();
        if (apiKey.isEmpty()) {
            chatDisplay->append("[error] API Key cannot be empty");
            return;
        }

        ai->setApiKey(apiKey);
        chatDisplay->append(QString("[ok] API Key set, length=%1").arg(apiKey.length()));
    });

    connect(apiKeyInput, &QLineEdit::returnPressed, setApiKeyBtn, &QPushButton::click);

    connect(serviceSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
        const auto type = static_cast<AiProvider::ServiceType>(serviceSelector->itemData(index).toInt());
        ai->setServiceType(type);
        modelSelector->clear();
        chatDisplay->append(QString("[config] service switched to %1").arg(serviceSelector->currentText()));
    });

    connect(fetchModelsBtn, &QPushButton::clicked, [=]() {
        if (apiKeyInput->text().trimmed().isEmpty()) {
            chatDisplay->append("[error] Set API Key first");
            return;
        }

        chatDisplay->append("[info] Fetching models...");
        fetchModelsBtn->setEnabled(false);
        modelSelector->clear();
        modelSelector->addItem("Loading...");
        ai->fetchModels();
    });

    connect(ai, &AiProvider::modelsReceived, [=](const QList<AiProvider::ModelInfo> &models) {
        modelSelector->clear();
        chatDisplay->append(QString("[ok] fetched %1 models").arg(models.size()));

        for (const auto &model : models) {
            QString displayText = model.id;
            if (!model.ownedBy.isEmpty()) {
                displayText += QString(" (%1)").arg(model.ownedBy);
            }

            modelSelector->addItem(displayText, model.id);
            chatDisplay->append(QString("  - %1").arg(model.id));
        }

        chatDisplay->append("");
        fetchModelsBtn->setEnabled(true);

        const QString currentModel = ai->currentModel();
        for (int i = 0; i < modelSelector->count(); ++i) {
            if (modelSelector->itemData(i).toString() == currentModel) {
                modelSelector->setCurrentIndex(i);
                break;
            }
        }
    });

    connect(modelSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
        if (index < 0 || modelSelector->itemData(index).isNull()) {
            return;
        }

        const QString modelId = modelSelector->itemData(index).toString();
        ai->setModel(modelId);
        chatDisplay->append(QString("[config] model switched to %1").arg(modelId));
    });

    connect(sendBtn, &QPushButton::clicked, [=]() {
        if (apiKeyInput->text().trimmed().isEmpty()) {
            chatDisplay->append("[error] Set API Key first");
            return;
        }

        const QString msg = input->text().trimmed();
        if (msg.isEmpty()) {
            return;
        }

        chatDisplay->append(QString("You: %1").arg(msg));
        input->clear();
        setChatBusy(true);

        const bool streamEnabled = ai->isStreamEnabled();
        ai->setProperty("streamReplyActive", streamEnabled);
        ai->setProperty("streamReplyHasChunks", false);

        if (streamEnabled) {
            chatDisplay->append("AI: ");
        }

        ai->chat(msg);
    });

    connect(input, &QLineEdit::returnPressed, sendBtn, &QPushButton::click);

    connect(ai, &AiProvider::replyChunkReceived, [=](const QString &chunk) {
        if (!ai->property("streamReplyActive").toBool()) {
            chatDisplay->append("AI: ");
            ai->setProperty("streamReplyActive", true);
        }

        ai->setProperty("streamReplyHasChunks", true);
        chatDisplay->moveCursor(QTextCursor::End);
        chatDisplay->insertPlainText(chunk);
        chatDisplay->ensureCursorVisible();
    });

    connect(ai, &AiProvider::replyReceived, [=](const QString &reply) {
        const bool streamActive = ai->property("streamReplyActive").toBool();
        const bool hasChunks = ai->property("streamReplyHasChunks").toBool();

        if (streamActive) {
            if (!hasChunks) {
                chatDisplay->moveCursor(QTextCursor::End);
                chatDisplay->insertPlainText(reply);
            }
            finishStreamUi();
        } else {
            chatDisplay->append(QString("AI: %1").arg(reply));
            chatDisplay->append("");
        }

        setChatBusy(false);
    });

    connect(ai, &AiProvider::errorOccurred, [=](const QString &error) {
        finishStreamUi();
        chatDisplay->append(QString("[error] %1").arg(error));
        chatDisplay->append("");
        setChatBusy(false);
        fetchModelsBtn->setEnabled(true);
    });

    chatDisplay->append("ZcAiLib example");
    chatDisplay->append("Click 'Fetch Models' to load available models.");
    chatDisplay->append("");
}

MainWindow::~MainWindow()
{
}

#include "mainwindow.h"

#include "aiprovider.h"
#include "ui_mainwindow.h"

#include <QTextCursor>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);

  AiProvider *ai = new AiProvider(this);
  ai->setServiceType(AiProvider::DeepSeek);
  ai->setStreamEnabled(true);
  ai->setProperty("streamReplyActive", false);
  ai->setProperty("streamReplyHasChunks", false);

  ui->serviceSelector->addItem("OpenAI", AiProvider::OpenAI);
  ui->serviceSelector->addItem("DeepSeek", AiProvider::DeepSeek);
  ui->serviceSelector->setCurrentIndex(1);
  ui->baseUrlInput->setText("https://api.deepseek.com/v1");

  auto setChatBusy = [=](bool busy) {
    ui->sendBtn->setEnabled(!busy);
    ui->input->setEnabled(!busy);
    if (!busy) {
      ui->input->setFocus();
    }
  };

  auto finishStreamUi = [=]() {
    if (!ai->property("streamReplyActive").toBool()) {
      return;
    }

    ui->chatDisplay->moveCursor(QTextCursor::End);
    ui->chatDisplay->insertPlainText("\n\n");
    ui->chatDisplay->ensureCursorVisible();
    ai->setProperty("streamReplyActive", false);
    ai->setProperty("streamReplyHasChunks", false);
  };

  connect(ui->systemPromptInput, &QLineEdit::textChanged, ai,
          &AiProvider::setSystemPrompt);

  connect(ui->streamCheckBox, &QCheckBox::toggled, [=](bool checked) {
    ai->setStreamEnabled(checked);
    ui->chatDisplay->append(
        QString("[config] stream %1").arg(checked ? "enabled" : "disabled"));
  });

  connect(ui->setApiKeyBtn, &QPushButton::clicked, [=]() {
    const QString apiKey = ui->apiKeyInput->text().trimmed();
    if (apiKey.isEmpty()) {
      ui->chatDisplay->append("[error] API Key cannot be empty");
      return;
    }

    ai->setApiKey(apiKey);
    ui->chatDisplay->append(
        QString("[ok] API Key set, length=%1").arg(apiKey.length()));
  });

  connect(ui->apiKeyInput, &QLineEdit::returnPressed, ui->setApiKeyBtn,
          &QPushButton::click);

  connect(ui->setBaseUrlBtn, &QPushButton::clicked, [=]() {
    const QString baseUrl = ui->baseUrlInput->text().trimmed();
    if (baseUrl.isEmpty()) {
      ui->chatDisplay->append("[error] Base URL cannot be empty");
      return;
    }

    ai->setBaseUrl(baseUrl);
    ui->chatDisplay->append(QString("[ok] Base URL set to %1").arg(baseUrl));
  });

  connect(ui->baseUrlInput, &QLineEdit::returnPressed, ui->setBaseUrlBtn,
          &QPushButton::click);

  connect(ui->serviceSelector,
          QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
            const auto type = static_cast<AiProvider::ServiceType>(
                ui->serviceSelector->itemData(index).toInt());
            ai->setServiceType(type);
            ui->modelSelector->clear();
            if (type == AiProvider::OpenAI) {
              ui->baseUrlInput->setText("https://api.openai.com/v1");
            } else if (type == AiProvider::DeepSeek) {
              ui->baseUrlInput->setText("https://api.deepseek.com/v1");
            }
            ui->chatDisplay->append(
                QString("[config] service switched to %1")
                    .arg(ui->serviceSelector->currentText()));
          });

  connect(ui->fetchModelsBtn, &QPushButton::clicked, [=]() {
    if (ui->apiKeyInput->text().trimmed().isEmpty()) {
      ui->chatDisplay->append("[error] Set API Key first");
      return;
    }

    ui->chatDisplay->append("[info] Fetching models...");
    ui->fetchModelsBtn->setEnabled(false);
    ui->modelSelector->clear();
    ui->modelSelector->addItem("Loading...");
    ai->fetchModels();
  });

  connect(ai, &AiProvider::modelsReceived,
          [=](const QList<AiProvider::ModelInfo> &models) {
            ui->modelSelector->clear();
            ui->chatDisplay->append(
                QString("[ok] fetched %1 models").arg(models.size()));

            for (const auto &model : models) {
              QString displayText = model.id;
              if (!model.ownedBy.isEmpty()) {
                displayText += QString(" (%1)").arg(model.ownedBy);
              }

              ui->modelSelector->addItem(displayText, model.id);
              ui->chatDisplay->append(QString("  - %1").arg(model.id));
            }

            ui->chatDisplay->append("");
            ui->fetchModelsBtn->setEnabled(true);

            const QString currentModel = ai->currentModel();
            for (int i = 0; i < ui->modelSelector->count(); ++i) {
              if (ui->modelSelector->itemData(i).toString() == currentModel) {
                ui->modelSelector->setCurrentIndex(i);
                break;
              }
            }
          });

  connect(ui->modelSelector,
          QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
            if (index < 0 || ui->modelSelector->itemData(index).isNull()) {
              return;
            }

            const QString modelId =
                ui->modelSelector->itemData(index).toString();
            ai->setModel(modelId);
            ui->chatDisplay->append(
                QString("[config] model switched to %1").arg(modelId));
          });

  connect(ui->sendBtn, &QPushButton::clicked, [=]() {
    if (ui->apiKeyInput->text().trimmed().isEmpty()) {
      ui->chatDisplay->append("[error] Set API Key first");
      return;
    }

    const QString msg = ui->input->text().trimmed();
    if (msg.isEmpty()) {
      return;
    }

    ui->chatDisplay->append(QString("You: %1").arg(msg));
    ui->input->clear();
    setChatBusy(true);

    const bool streamEnabled = ai->isStreamEnabled();
    ai->setProperty("streamReplyActive", streamEnabled);
    ai->setProperty("streamReplyHasChunks", false);

    if (streamEnabled) {
      ui->chatDisplay->append("AI: ");
    }

    ai->chat(msg);
  });

  connect(ui->input, &QLineEdit::returnPressed, ui->sendBtn,
          &QPushButton::click);

  connect(ai, &AiProvider::replyChunkReceived, [=](const QString &chunk) {
    if (!ai->property("streamReplyActive").toBool()) {
      ui->chatDisplay->append("AI: ");
      ai->setProperty("streamReplyActive", true);
    }

    ai->setProperty("streamReplyHasChunks", true);
    ui->chatDisplay->moveCursor(QTextCursor::End);
    ui->chatDisplay->insertPlainText(chunk);
    ui->chatDisplay->ensureCursorVisible();
  });

  connect(ai, &AiProvider::replyReceived, [=](const QString &reply) {
    const bool streamActive = ai->property("streamReplyActive").toBool();
    const bool hasChunks = ai->property("streamReplyHasChunks").toBool();

    if (streamActive) {
      if (!hasChunks) {
        ui->chatDisplay->moveCursor(QTextCursor::End);
        ui->chatDisplay->insertPlainText(reply);
      }
      finishStreamUi();
    } else {
      ui->chatDisplay->append(QString("AI: %1").arg(reply));
      ui->chatDisplay->append("");
    }

    setChatBusy(false);
  });

  connect(ai, &AiProvider::errorOccurred, [=](const QString &error) {
    finishStreamUi();
    ui->chatDisplay->append(QString("[error] %1").arg(error));
    ui->chatDisplay->append("");
    setChatBusy(false);
    ui->fetchModelsBtn->setEnabled(true);
  });

  ui->chatDisplay->append("ZcAiLib example");
  ui->chatDisplay->append("Click 'Fetch Models' to load available models.");
  ui->chatDisplay->append("");
}

MainWindow::~MainWindow() { delete ui; }

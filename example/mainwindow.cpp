#include "mainwindow.h"
#include "AiProvider.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  AiProvider *ai = new AiProvider(this);
  ai->setServiceType(AiProvider::DeepSeek);

  // UI 组件
  QWidget *central = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(central);

  // === API Key 输入 ===
  QHBoxLayout *apiKeyLayout = new QHBoxLayout;
  QLabel *apiKeyLabel = new QLabel("API Key：");
  QLineEdit *apiKeyInput = new QLineEdit;
  apiKeyInput->setEchoMode(QLineEdit::Password);
  apiKeyInput->setPlaceholderText("请输入你的 API Key");
  QPushButton *setApiKeyBtn = new QPushButton("✅ 设置");
  setApiKeyBtn->setMaximumWidth(60);

  apiKeyLayout->addWidget(apiKeyLabel);
  apiKeyLayout->addWidget(apiKeyInput, 1);
  apiKeyLayout->addWidget(setApiKeyBtn);
  layout->addLayout(apiKeyLayout);

  // === 服务选择 ===
  QHBoxLayout *serviceLayout = new QHBoxLayout;
  QLabel *serviceLabel = new QLabel("AI 服务：");
  QComboBox *serviceSelector = new QComboBox;
  serviceSelector->addItem("OpenAI", AiProvider::OpenAI);
  serviceSelector->addItem("DeepSeek", AiProvider::DeepSeek);
  serviceSelector->setCurrentIndex(1); // 默认 DeepSeek

  serviceLayout->addWidget(serviceLabel);
  serviceLayout->addWidget(serviceSelector, 1);

  // === 模型选择 ===
  QHBoxLayout *modelLayout = new QHBoxLayout;
  QLabel *modelLabel = new QLabel("模型：");
  QComboBox *modelSelector = new QComboBox;
  modelSelector->setMinimumWidth(300);
  QPushButton *fetchModelsBtn = new QPushButton("🔄 获取模型列表");

  modelLayout->addWidget(modelLabel);
  modelLayout->addWidget(modelSelector, 1);
  modelLayout->addWidget(fetchModelsBtn);

  // === 系统提示词 ===
  QHBoxLayout *systemPromptLayout = new QHBoxLayout;
  QLabel *systemPromptLabel = new QLabel("系统提示词：");
  QLineEdit *systemPromptInput = new QLineEdit;
  systemPromptInput->setPlaceholderText("例如：你是一位幽默风趣的聊天助手");
  systemPromptLayout->addWidget(systemPromptLabel);
  systemPromptLayout->addWidget(systemPromptInput, 1);
  layout->addLayout(systemPromptLayout);

  // === 聊天区域 ===
  QTextEdit *chatDisplay = new QTextEdit;
  chatDisplay->setReadOnly(true);

  QHBoxLayout *inputLayout = new QHBoxLayout;
  QLineEdit *input = new QLineEdit;
  input->setPlaceholderText("输入消息...");
  QPushButton *sendBtn = new QPushButton("发送");

  inputLayout->addWidget(input);
  inputLayout->addWidget(sendBtn);

  // 绑定系统提示词设置
  connect(systemPromptInput, &QLineEdit::textChanged, [=](const QString &text) {
    ai->setSystemPrompt(text);
    chatDisplay->append(QString("⚙️ 系统提示词已更新：%1").arg(text));
  });
  // === 添加到主布局 ===
  layout->addLayout(serviceLayout);
  layout->addLayout(modelLayout);
  layout->addWidget(new QLabel("💬 对话："));
  layout->addWidget(chatDisplay);
  layout->addLayout(inputLayout);

  setCentralWidget(central);
  resize(800, 600);
  setWindowTitle("ZcAiLib 示例");

  // ========== 信号连接 ==========

  // 设置 API Key
  connect(setApiKeyBtn, &QPushButton::clicked, [=]() {
    QString apiKey = apiKeyInput->text().trimmed();
    if (apiKey.isEmpty()) {
      chatDisplay->append("❌ API Key 不能为空");
      return;
    }
    ai->setApiKey(apiKey);
    chatDisplay->append(
        QString("✅ API Key 已设置（长度：%1）").arg(apiKey.length()));
    setApiKeyBtn->setText("✓ 设置");
  });

  // 回车快速设置 API Key
  connect(apiKeyInput, &QLineEdit::returnPressed, setApiKeyBtn,
          &QPushButton::click);

  // 切换服务
  connect(
      serviceSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
      [=](int index) {
        AiProvider::ServiceType type = static_cast<AiProvider::ServiceType>(
            serviceSelector->itemData(index).toInt());
        ai->setServiceType(type);
        modelSelector->clear();
        chatDisplay->append(
            QString("✅ 已切换到：%1\n").arg(serviceSelector->currentText()));
      });

  // 获取模型列表
  connect(fetchModelsBtn, &QPushButton::clicked, [=]() {
    if (apiKeyInput->text().trimmed().isEmpty()) {
      chatDisplay->append("❌ 请先设置 API Key");
      return;
    }
    chatDisplay->append("🔄 正在从 API 获取模型列表...");
    fetchModelsBtn->setEnabled(false);
    modelSelector->clear();
    modelSelector->addItem("加载中...");
    ai->fetchModels();
  });

  // 接收模型列表
  connect(ai, &AiProvider::modelsReceived,
          [=](const QList<AiProvider::ModelInfo> &models) {
            modelSelector->clear();

            chatDisplay->append(
                QString("✅ 成功获取 %1 个模型：").arg(models.size()));

            for (const auto &model : models) {
              // 显示格式：模型ID (拥有者)
              QString displayText = model.id;
              if (!model.ownedBy.isEmpty()) {
                displayText += QString(" (%1)").arg(model.ownedBy);
              }

              modelSelector->addItem(displayText, model.id);
              chatDisplay->append(QString("  • %1").arg(model.id));
            }

            chatDisplay->append(""); // 空行
            fetchModelsBtn->setEnabled(true);

            // 自动选择当前模型
            QString currentModel = ai->currentModel();
            for (int i = 0; i < modelSelector->count(); ++i) {
              if (modelSelector->itemData(i).toString() == currentModel) {
                modelSelector->setCurrentIndex(i);
                break;
              }
            }
          });

  // 切换模型
  connect(modelSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [=](int index) {
            if (index >= 0 && !modelSelector->itemData(index).isNull()) {
              QString modelId = modelSelector->itemData(index).toString();
              ai->setModel(modelId);
              chatDisplay->append(QString("✅ 已切换模型：%1\n").arg(modelId));
            }
          });

  // 发送消息
  connect(sendBtn, &QPushButton::clicked, [=]() {
    if (apiKeyInput->text().trimmed().isEmpty()) {
      chatDisplay->append("❌ 请先设置 API Key");
      return;
    }
    QString msg = input->text().trimmed();
    if (msg.isEmpty())
      return;

    chatDisplay->append(QString("👤 你：%1").arg(msg));
    input->clear();
    sendBtn->setEnabled(false);
    input->setEnabled(false);
    ai->chat(msg);
  });

  // 回车发送
  connect(input, &QLineEdit::returnPressed, sendBtn, &QPushButton::click);

  // 接收回复
  connect(ai, &AiProvider::replyReceived, [=](const QString &reply) {
    chatDisplay->append(QString("🤖 AI：%1").arg(reply));
    chatDisplay->append(""); // 空行
    sendBtn->setEnabled(true);
    input->setEnabled(true);
    input->setFocus();
  });

  // 错误处理
  connect(ai, &AiProvider::errorOccurred, [=](const QString &error) {
    chatDisplay->append(error);
    chatDisplay->append("");
    sendBtn->setEnabled(true);
    input->setEnabled(true);
    fetchModelsBtn->setEnabled(true);
  });

  // 启动提示
  chatDisplay->append("👋 欢迎使用 ZcAiLib！");
  chatDisplay->append("📝 点击「🔄 获取模型列表」开始\n");
}

MainWindow::~MainWindow() {}

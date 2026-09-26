#include <AiProvider.h>
#include <QCoreApplication>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    AiProvider provider;
    provider.setServiceType(AiProvider::Custom);
    provider.setModel(QStringLiteral("consumer-model"));
    return provider.currentModel() == QStringLiteral("consumer-model") ? 0 : 1;
}

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QDateTime>

class EconServer : public QObject
{

public:
    explicit EconServer(QObject *parent = nullptr) : QObject(parent)
    {
        startEventServer();
        connectMt5Bridge();
    }

private:
    QTcpServer eventServer;     // Python fetcher connects here
    QTcpSocket mt5Socket;       // Python MT5 bridge

    // ---------------- EVENT SERVER ----------------
    void startEventServer()
    {
        connect(&eventServer, &QTcpServer::newConnection, this, [&]() {
            QTcpSocket *client = eventServer.nextPendingConnection();
            qDebug() << "📥 Event source connected";

            connect(client, &QTcpSocket::readyRead, this, [this, client]() {
                while (client->canReadLine()) {
                    QByteArray line = client->readLine().trimmed();
                    handleEvent(line);
                }
            });

            connect(client, &QTcpSocket::disconnected, this, [client]() {
                qDebug() << "❌ Event source disconnected";
                client->deleteLater()
            });

            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                while (socket->canReadLine()) {
                    QByteArray line = socket->readLine().trimmed();
                    QJsonDocument doc = QJsonDocument::fromJson(line);

                    if (!doc.isObject())
                        continue;

                    handleIncomingEvents(doc.object());
                }
            })
        });

        if (!eventServer.listen(QHostAddress::Any, 9000)) {
            qFatal("Failed to listen on port 9000");
        }

        qDebug() << "🟢 Econ server listening on port 9000";
    }

    // ---------------- MT5 BRIDGE ----------------
    void connectMt5Bridge()
    {
        connect(&mt5Socket, &QTcpSocket::connected, this, []() {
            qDebug() << "🟢 Connected to MT5 bridge";
        });

        connect(&mt5Socket, &QTcpSocket::errorOccurred, this,
                [&](QAbstractSocket::SocketError) {
                    qDebug() << "⚠ MT5 bridge error:" << mt5Socket.errorString();
                });

        mt5Socket.connectToHost("127.0.0.1", 9100);
    }

    // ---------------- INCOMING EVENTS HANDLER -------
    void handleIncomingEvents(const QJsonObject &event)
    {
        QString name = event["event"].toString();
        QString symbol = event["symbol"].toString();
        double forecast = event["forecast"].toDouble();
        double actual = event["actual"].toDouble();

        if (forecast == 0)
            return;

        double surprise = calculateSuprise(actual, forecast);
        QString signal = decideTrade(surprise);

        qDebug() << "EVENT:" << name
                 << "Forecast:" << forecast
                 << "Actual:" << actual
                 << "Surprise:" << surprise
                 << "Signal:" << signal;
        if (signal != "NONE") {
            executeTrade(symbol, signal, surprise);
        }
    }

    // ---------------- EVENT HANDLING ----------------
    void handleEvent(const QByteArray &jsonLine)
    {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(jsonLine, &err);

        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            qDebug() << "Invalid JSON event";
            return;
        }

        QJsonObject e = doc.object();

        QString symbol   = e["symbol"].toString();
        double forecast  = e["forecast"].toDouble();
        double actual    = e["actual"].toDouble();

        if (forecast == 0.0) return;

        double surprise = actual - forecast;
        QString decision = decideTrade(surprise);

        qDebug()
            << "📊 Event" << symbol
            << "Forecast:" << forecast
            << "Actual:" << actual
            << "Surprise:" << surprise
            << "Decision:" << decision;

        if (decision != "NONE") {
            sendTrade(symbol, decision);
        }
    }

    // ---------------- STRATEGY ----------------
    QString decideTrade(double surprise)
    {
        const double threshold = 0.2;

        if (surprise >= threshold)  return "BUY";
        if (surprise <= -threshold) return "SELL";
        return "NONE";
    }

    // ---------------- SEND TO MT5 ----------------
    void sendTrade(const QString &symbol, const QString &direction)
    {
        if (mt5Socket.state() != QAbstractSocket::ConnectedState) {
            qDebug() << "⚠ MT5 bridge not connected";
            return;
        }

        QJsonObject trade;
        trade["symbol"] = symbol;
        trade["direction"] = direction;
        trade["lot"] = 0.1;

        QJsonDocument doc(trade);
        mt5Socket.write(doc.toJson(QJsonDocument::Compact) + "\n");
        mt5Socket.flush();

        qDebug() << "📤 Trade sent to MT5:" << direction << symbol;
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    EconServer server;
    return app.exec();
}

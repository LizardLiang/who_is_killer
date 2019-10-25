#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QTcpServer>
#include <QStringListModel>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>

#include "../Famax/MotorTest/testIO/logsystem.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class Player{
public:
    QString name;
    QTcpSocket* m_socket;
    int character;
};

class Msg_queue{
public:
    QString type = nullptr;
    QTcpSocket* m_socket = nullptr;
    QString msg = nullptr;
};

class Cmd_queue{
public:
    QString cmd = nullptr;
    QTcpSocket* m_socket = nullptr;
    QString msg = nullptr;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    logsystem *m_log = new logsystem;

    QTcpServer *m_server;           //創建一個server
    QList<Player> players;          //創建一個用戶列表
    QStringList name_list = QStringList();
    QStringListModel *m_model = new QStringListModel();

    QList<Msg_queue> msg_list = QList<Msg_queue>();
    QList<Cmd_queue> cmd_list = QList<Cmd_queue>();

    QTimer *m_timer = new QTimer();
    QTimer *msg_timer = new QTimer();
    QTimer *cmd_timer = new QTimer();

    void startserver();             //start server

    //發送訊息
    QString create_msg(QString, QString);
    void public_broadcast(QString, QTcpSocket *socket); //當有玩家要發送全體訊息
    void single_broadcast(QString, QTcpSocket *socket); //當需要發送單獨玩家訊息
    void system_broadcast(QString);                     //當有需要發送系統訊息

    void add_player(QString, QTcpSocket *);             //新玩家加入遊戲時
    void add_queue(QString type, QTcpSocket *socket, QString msg);
    void add_command(QString type, QTcpSocket *socket, QString msg);
    void set_ready(QString);

    int ready_cnt = 0;

    void assign_character();
    int cha_num = 0;
    enum role{
        civi_1 = 1,
        civi_2,
        civi_3,
        killer_1,
        killer_2,
        killer_3,
        god_1,
        god_2,
        god_3,
        civi_4
    };


private slots:
    void new_connection();          //when user connect to server

    void read_message();            //when server received a message from client
    void timeoutdeal();             //when tick time arrive
    void msg_timeout();
    void command_timeout();

signals:
    void add_log(QString);
};
#endif // MAINWINDOW_H

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(this, SIGNAL(add_log(QString)), m_log, SLOT(add_log(QString)));
    name_list = QStringList();
    m_model->setStringList(name_list);
    ui->listView->setModel(m_model);

    m_timer->setInterval(100);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(timeoutdeal()));

    msg_timer->setInterval(50);
    connect(msg_timer, SIGNAL(timeout()), this, SLOT(msg_timeout()));

    cmd_timer->setInterval(50);
    connect(cmd_timer, SIGNAL(timeout()), this, SLOT(command_timeout()));

    startserver();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::startserver(){
    //啟動server
    m_server = new QTcpServer(this);

    m_server->listen(QHostAddress::Any, 19999);
    if(m_server->isListening())
        emit add_log("server is on");
    else
        emit add_log("Server can't listen");

    connect(m_server, SIGNAL(newConnection()), this, SLOT(new_connection()));
}

void MainWindow::new_connection(){
    //當建立新的連線之後
    Player player = Player();
    QTcpSocket *m_socket = m_server->nextPendingConnection();

    player.m_socket = m_socket;
    players.push_back(player);

    connect(m_socket, &QTcpSocket::readyRead, this, &MainWindow::read_message);

    QString msg = QString("Server got a new connection. From %1:%2")
            .arg(m_socket->peerAddress().toIPv4Address()).arg(m_socket->peerPort());
    emit add_log(msg);

    QJsonObject msg_json = QJsonObject();
    msg_json.insert("ASK", "NAME");
    QJsonDocument doc(msg_json);
    QString msg_s = create_msg("ASK", "NAME");

    //QString msg_s = QString("/ASK");
    add_queue("single", m_socket, msg_s);
    emit add_log("request for their name");
}

void MainWindow::read_message(){
    //當收到訊息從這裡做分配
    QTcpSocket* socket = static_cast<QTcpSocket*>(QObject::sender());  // 获得是哪个socket收到了消息

    QString str = socket->readAll().data();

    //發送接受的格式用Json
    QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8());
    if(!doc.isObject()){
        QString msg = QString("Error, msg format is incorrect");
        emit add_log(msg);
        return;
    }
    QJsonObject obj = doc.object();

    //收到玩家名稱
    foreach(QString key, obj.keys()){
        if(key != "msg")
            add_command(key, socket, obj[key].toString());
        else
            add_queue("system", nullptr, str);
    }

    //系統log
    QString msg = QString("Received message %1, From %2").arg(str)
            .arg(socket->localAddress().toIPv4Address());
    emit add_log(msg);
    ui->textEdit->append(str);
}

void MainWindow::public_broadcast(QString data, QTcpSocket *socket){
    //送訊息給全體玩家除了特定玩家
    for(int i=0; i<players.size(); i++){
        if(players[i].m_socket->peerAddress().toIPv4Address() !=
                socket->peerAddress().toIPv4Address())
            players[i].m_socket->write(data.toStdString().data());
    }
}

void MainWindow::add_player(QString str, QTcpSocket *socket){
    //當有新玩家加入
    for(int i = 0; i < players.size(); i++) {
        //先去找到這個玩家的IP紀錄
        Player player = players[i];
        if(player.m_socket->peerAddress().toIPv4Address()
                == socket->peerAddress().toIPv4Address()){
            player.name = str;

            //這個function 是為了要讓新加入的玩家可以得到已加入的玩家資訊
            QString msg_2 = nullptr;
            foreach(QString name, name_list){
                msg_2.append(name);
                msg_2.append(",");
            }
            msg_2.append(player.name);
//            QJsonObject obj_c = QJsonObject();
//            obj_c.insert("ADDOLDPLAYER", msg_2);
//            QJsonDocument doc(obj_c);
            QString msg_c = create_msg("ADDOLDPLAYER", msg_2);
            add_queue("single", socket, msg_c);
            QString msg_3 = QString("Sending exist player to %1")
                    .arg(player.name);
            emit add_log(msg_3);

            //這裡開始是將新玩家加入的訊息通知其他玩家
            name_list.append(player.name);
            m_model->setStringList(name_list);
            ui->listView->setModel(m_model);
//            QJsonObject obj = QJsonObject();
//            obj.insert("ADDPLAYER", player.name);
//            QJsonDocument doc_c(obj);
            QString msg_1 = create_msg("ADDPLAYER", player.name);
            add_queue("public", socket, msg_1);
            if(!m_timer->isActive())
                m_timer->start();
            players[i] = player;

            //系統log
            QString msg = QString("New player %2 has join into game")
                    .arg(players[i].name);
            emit add_log(msg);
            return;
        }
    }
}

void MainWindow::single_broadcast(QString data, QTcpSocket *socket){
    //只送訊息給特定玩家
    socket->write(data.toStdString().data());
}

void MainWindow::system_broadcast(QString data){
    //發送系統訊息
    for(int i = 0; i < players.size(); i++){
        Player player = players[i];
        qint64 rec = player.m_socket->write(data.toStdString().data());
        if(rec == -1){
            //若無法傳送則代表玩家已斷線
            //將該玩家從玩家列表當中去除
            QString msg = QString("Player %1 is disconnected").arg(player.name);
            const QString re_name = player.name;
            name_list.removeOne(re_name);
            m_model->setStringList(name_list);
            ui->listView->setModel(m_model);
            players.removeAt(i);

            //通知其他玩家這個玩家已經斷線惹
//            QJsonObject obj = QJsonObject();
//            obj.insert("RemovePlayer", player.name);
//            QJsonDocument doc(obj);

            //發送系統訊息
            QString msg_s = create_msg("RemovePlayer", player.name);
            add_queue("system", nullptr, msg_s);
            add_log(msg);
        }
    }
}

void MainWindow::timeoutdeal(){
    //每隔0.1秒去ping 一次玩家看他還在ㄇ
    m_timer->stop();
    QJsonObject obj = QJsonObject();
    obj.insert("Ping", "");
    QJsonDocument doc(obj);
    QString msg = QString::fromUtf8(doc.toJson());
    add_queue("system", nullptr, msg);
    if(!players.isEmpty())
        m_timer->start();
}

void MainWindow::add_queue(QString type, QTcpSocket *socket, QString msg){
    //用排隊的方式去發送訊息
    Msg_queue m_queue = Msg_queue();
    m_queue.type = type;
    m_queue.m_socket = socket;
    m_queue.msg = msg;

    msg_list.append(m_queue);
    if(!msg_timer->isActive())
        msg_timer->start();
}

void MainWindow::msg_timeout(){
    //每隔0.01秒發送一次訊息
    msg_timer->stop();
    if(msg_list.isEmpty())
        return;
    Msg_queue m_q = msg_list.first();
    msg_list.pop_front();
    if(m_q.type == "single"){
        single_broadcast(m_q.msg, m_q.m_socket);
    }
    else if(m_q.type == "public"){
        public_broadcast(m_q.msg, m_q.m_socket);
    }
    else if(m_q.type == "system"){
        system_broadcast(m_q.msg);
    }
    if(!msg_list.isEmpty())
        msg_timer->start();
}

void MainWindow::command_timeout(){
    cmd_timer->stop();
    if(cmd_list.isEmpty())
        return;
    Cmd_queue cmd = cmd_list.first();
    cmd_list.pop_front();
    if(cmd.cmd == "NAME"){
        add_player(cmd.msg, cmd.m_socket);
    }
    else if(cmd.cmd == "setcharacter"){
        assign_character();
    }
    else if(cmd.cmd == "ready"){
        set_ready(cmd.msg);
    }
    cmd_timer->start();
}

void MainWindow::add_command(QString type, QTcpSocket *socket, QString msg){
    Cmd_queue cmd_q = Cmd_queue();
    cmd_q.cmd = type;
    cmd_q.m_socket = socket;
    cmd_q.msg = msg;

    cmd_list.append(cmd_q);
    if(!cmd_timer->isActive())
        cmd_timer->start();
    QString msg_log = QString("Cmd %1 is added").arg(type);
    emit add_log(msg_log);
}

void MainWindow::assign_character(){
    //    if(players.size() == 9){
    //        int array[9] = {0};
    //        QList<int> tmp = {1,2,3,4,5,6,7,8,9};
    //        for(int i = 0; i <= 8; i++){
    //            qsrand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch()));
    //            int role_int = qrand()%((tmp.size()+0)-0)+0;
    //            array[i] = tmp[role_int];
    //            tmp.removeAt(role_int);
    //            qDebug() << array[i];
    //        }
    //    }
    int array[9] = {0};
    QList<int> tmp = {1,2,3,4,5,6,7,8,9};
    for(int i = 0; i <= 8; i++){
        qsrand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch()));
        int role_int = qrand()%((tmp.size()+0)-0)+0;
        array[i] = tmp[role_int];
        tmp.removeAt(role_int);
        qDebug() << array[i];
    }
    for(int i = 0; i < players.size(); i++){
        players[i].character = array[i];
        QJsonObject obj = QJsonObject();
        obj.insert("setcharacter", players[i].character);
        QJsonDocument doc(obj);
        QString msg = QString::fromUtf8(doc.toJson());
        add_queue("single", players[i].m_socket, msg);
        QString msg_log = QString("Player %1's character is %2")
                .arg(players[i].name).arg(players[i].character);
        emit add_log(msg_log);
    }
}

void MainWindow::set_ready(QString data){
    if(data == "true")
        ready_cnt += 1;
    else
        ready_cnt -= 1;
    if(ready_cnt == players.size()){
        add_command("setcharacter", nullptr, nullptr);
        QJsonObject obj = QJsonObject();
        obj.insert("set_start", 1);
        QJsonDocument doc(obj);
        QString msg = QString::fromUtf8(doc.toJson());
        add_queue("system", nullptr, msg);
    }
    QString msg_log = QString("Ready player count %1").arg(ready_cnt);
}

QString MainWindow::create_msg(QString key, QString msg){
    QJsonObject obj = QJsonObject();
    obj.insert(key, msg);
    QJsonDocument doc(obj);
    QString msg_ = QString::fromUtf8(doc.toJson());
    return msg_;
}

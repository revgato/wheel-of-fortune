extern "C"
{
#include "utils.h"
}
#include "backend.h"

extern conn_msg_type conn_msg;
extern char username[50];
extern int bytes_received;
extern int bytes_sent;
extern int my_turn;
Backend *Backend::instance = nullptr;

// pthread_mutex_t lock;

// QString Backend::text = "";
QStringList Backend::textList;
char Backend::server_ip[16];
int Backend::server_port = 5500;

Backend::Backend(QObject *parent) : QObject(parent)
{
    if (Backend::instance == nullptr)
    {
        Backend::instance = this;
    }
}

Backend::~Backend()
{
    delete Backend::instance;
}

void Backend::connectToServer()
{
    // pthread_mutex_init(&lock, NULL);
    if (bytes_received > 0){
        return;
    }
    
    if (connect_to_server(Backend::server_ip, Backend::server_port) == 1)
    {
        emit connectedToServer();
    }
    else
    {
        emit connectionFailed();
    }
}

void Backend::join(QString username_input)
{
    textList.clear();
    strcpy(username, username_input.toStdString().c_str());
    conn_msg.data.player = init_player(username, -1);
    conn_msg.type = JOIN;
    send_server();
    receive_server();
    pthread_t tid;

    if (conn_msg.type == WAITING_ROOM)
    {
        for (int i = 0; i < conn_msg.data.waiting_room.joined; i++)
        {
            textList.append(conn_msg.data.waiting_room.player[i].username);
        }
        // Backend::text = QString::fromStdString(conn_msg.data.waiting_room.player[0].username);
        emit waitingRoom();
        pthread_create(&tid, NULL, &pthread_waiting_room, NULL);
    }
    else
    {
        textList.clear();
        textList.append(QString::fromStdString(conn_msg.data.notification));
        emit refuse();
    }
}

void Backend::startGame()
{
    pthread_t tid;
    pthread_create(&tid, NULL, &pthread_game_state, NULL);
}

void Backend::updateWaitingRoom()
{
    textList.clear();
    for (int i = 0; i < conn_msg.data.waiting_room.joined; i++)
    {
        textList.append(conn_msg.data.waiting_room.player[i].username);
    }
    sleep(SLEEP_TIME);
    emit waitingRoom();
}

void Backend::updateGameState()
{
    textList.clear();
    textList.append(QString::fromStdString(conn_msg.data.game_state.crossword));
    textList.append(QString::number(conn_msg.data.game_state.sector));
    textList.append(QString::fromStdString(conn_msg.data.game_state.player[conn_msg.data.game_state.turn].username));
    for (int i = 0; i < PLAYER_PER_ROOM; i++)
    {
        textList.append(conn_msg.data.game_state.player[i].username);
        // textList.append(conn_msg.data.game_state.player[i].username);
        textList.append(QString::number(conn_msg.data.game_state.player[i].point));
        printf("Player %s has %d points\n", textList.at(1).toStdString().c_str(), textList.at(2).toInt());
    }
    if (my_turn)
    {
        textList.append("my_turn");
    }
    emit gameState();
}

void Backend::updateNotification()
{
    textList.clear();
    textList.append(QString::fromStdString(conn_msg.data.notification));
    emit notification();
}

void Backend::updateSubQuestion()
{
    textList.clear();
    textList.append(QString::fromStdString(conn_msg.data.sub_question.question));
    textList.append(QString::fromStdString(conn_msg.data.sub_question.answer[0]));
    textList.append(QString::fromStdString(conn_msg.data.sub_question.answer[1]));
    textList.append(QString::fromStdString(conn_msg.data.sub_question.answer[2]));
    textList.append(QString::fromStdString(conn_msg.data.sub_question.username));
    if (my_turn)
    {
        textList.append("my_turn");
    }
    emit subQuestion();
}

void Backend::updateEndGame()
{
    char last_line[100];
    int j;
    char *poiter_last_line;
    char winner[50];
    char *poiter_winner;
    textList.clear();
    textList.append(QString::fromStdString(conn_msg.data.game_state.crossword));
    printf("\n\n\nrossword: %s\n\n\n", conn_msg.data.game_state.crossword);
    for (int i = 0; i < PLAYER_PER_ROOM; i++)
    {
        textList.append(conn_msg.data.game_state.player[i].username);
        textList.append(QString::number(conn_msg.data.game_state.player[i].point));
        printf("Player %s has %d points\n", textList.at(1).toStdString().c_str(), textList.at(2).toInt());
    }
    // last_line is last line of conn_msg.data.game_state.game_message
    for (int i = 0; i < strlen(conn_msg.data.game_state.game_message) - 2; i++)
    {
        if (conn_msg.data.game_state.game_message[i] == '\n')
        {
            poiter_last_line = &conn_msg.data.game_state.game_message[i + 1];
        }
    }
    strcpy(last_line, poiter_last_line);

    // Winner is last word of last_line
    j = 0;
    for (int i = 0; i < strlen(last_line) - 1; i++)
    {
        if (last_line[i] == ' ')
        {
            poiter_winner = &last_line[i + 1];
        }
    }
    strcpy(winner, poiter_winner);
    textList.append(QString::fromStdString(winner));
    emit endGame();
    // TODO: configure server.c and the winner
}

void Backend::guessChar(QString guess)
{
    char guess_char = toupper(guess.toStdString().c_str()[0]);
    // If the guess is not a letter
    if (guess_char < 'A' || guess_char > 'Z')
    {
        if (textList.size() > 10)
        {
            textList.removeLast();
        }
        // textList.clear();
        textList.append("CHỈ ĐƯỢC NHẬP CHỮ CÁI");
        emit gameState();
        return;
    }

    for(int i = 0; i < strlen(conn_msg.data.game_state.crossword); i++)
    {
        if (guess_char == conn_msg.data.game_state.crossword[i])
        {
            if (textList.size() > 10)
            {
                textList.removeLast();
            }
            printf("Before append: %d\n", textList.size());
            // textList.clear();
            textList.append("CHỮ CÁI ĐÃ ĐƯỢC ĐOÁN");
            printf("After append: %d\n", textList.size());
            emit gameState();
            return;
        }
    }

    conn_msg.data.game_state.guess_char = toupper(guess.toStdString().c_str()[0]);
    conn_msg.type = GUESS_CHAR;
    send_server();
    if (bytes_sent <= 0)
    {
        printf("Server disconnected\n");
        emit connectionFailed();
    }
}

void Backend::guessCharSubQuestion(QString guess)
{
    char guess_char = toupper(guess.toStdString().c_str()[0]);
    // If the guess is not A, B or C
    if (guess_char < 'A' || guess_char > 'C')
    {
        if (textList.size() > 6)
        {
            textList.removeLast();
        }
        // textList.clear();
        textList.append("CHỈ ĐƯỢC NHẬP A, B HOẶC C");
        emit subQuestion();
        return;
    }
    conn_msg.data.sub_question.guess = toupper(guess.toStdString().c_str()[0]);
    conn_msg.type = SUB_QUESTION;
    send_server();
    if (bytes_sent <= 0)
    {
        printf("Server disconnected\n");
        emit connectionFailed();
    }
}

void Backend::exitGame()
{
    exit(0);
}

void *pthread_waiting_room(void *arg)
{
    free(arg);
    pthread_detach(pthread_self());
    while (conn_msg.data.waiting_room.joined != PLAYER_PER_ROOM)
    {
        receive_server();
        // if(bytes_received <=0 ){
        //     printf("Server disconnected\n");
        //     emit Backend::instance->connectionFailed();
        //     pthread_cancel(pthread_self());
        // }

        if (conn_msg.type == WAITING_ROOM)
        {
            printf("New player joined\n");
            emit Backend::instance->userJoined();
        }
    }
    printf("All players joined\n");
    emit Backend::instance->gameStart();
    // emit Backend::instance->waitingRoom();
    pthread_cancel(pthread_self());
}

void *pthread_game_state(void *arg)
{
    free(arg);
    pthread_detach(pthread_self());
    while (1)
    {
        sleep(SLEEP_TIME);
        receive_server();
        if (bytes_received <= 0)
        {
            emit Backend::instance->connectionFailed();
            pthread_cancel(pthread_self());
        }
        if (conn_msg.type == GAME_STATE)
        {
            printf("Game state received\n");
            // If current player is my turn
            if (strcmp(conn_msg.data.game_state.player[conn_msg.data.game_state.turn].username, username) == 0)
            {
                my_turn = 1;
            }
            else
            {
                my_turn = 0;
            }
            emit Backend::instance->updateGameStateSignal();
        }
        else if (conn_msg.type == NOTIFICATION)
        {
            printf("Notification received: %s\n", conn_msg.data.notification);
            emit Backend::instance->updateNotificationSignal();
        }
        else if (conn_msg.type == SUB_QUESTION)
        {
            printf("Sub question received: %s\n", conn_msg.data.sub_question.question);
            // If current player is my turn
            if (strcmp(conn_msg.data.sub_question.username, username) == 0)
            {
                my_turn = 1;
            }
            else
            {
                my_turn = 0;
            }
            emit Backend::instance->updateSubQuestionSignal();
        }
        else if (conn_msg.type == END_GAME)
        {
            printf("Game ended\n");
            emit Backend::instance->updateEndGameSignal();
            sleep(5);
            break;
        }
    }
    pthread_cancel(pthread_self());
}
// pthread_mutex_destroy(&lock);

QStringList Backend::getTextList() const
{
    return textList;
}

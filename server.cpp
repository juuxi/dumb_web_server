#include <thread>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <queue>
#include <string>
#include <iostream>


int flag_rcv = 0;
int flag_process = 0;
int flag_wait = 0;
int client_sock;
int listen_sock;
struct sockaddr_in addr;
struct sockaddr_in addr2;
pthread_mutex_t mutex;
pthread_t id1, id2, id3;
std::thread *t1 = nullptr, *t2 = nullptr;
std::queue<std::string> q;

void func1(void* arg) {
    int client_fd = *((int*)arg);
    free(((int*)arg));

    printf("поток приема начал работу\n");
    while (flag_rcv == 0) {
        char rcv_msg[256];
        int rv = recv(client_fd, rcv_msg, sizeof(rcv_msg), 0);
        if (rv == -1) {
            perror("receive");
            sleep(1);
        } else if (rv == 0) {
            shutdown(client_fd, 0);
            break;
        } else {
            pthread_mutex_lock(&mutex);
            q.push(std::string(rcv_msg, rv));
            pthread_mutex_unlock(&mutex);
        }
    }
    close(client_fd);
    printf("поток приема закончил работу\n");
}

void func2(void* arg) {
    int client_fd = *((int*)arg);
    printf("поток обработки начал работу\n");

    while (flag_process == 0) {
        pthread_mutex_lock(&mutex);
        if (!q.empty()) {
            std::string first_ent = q.front();
            q.pop();
            pthread_mutex_unlock(&mutex);

            std::cout << "Сообщение\n" << first_ent << "Принято" << std::endl;
            std::string http_method = first_ent.substr(0, first_ent.find(' '));
            char send_msg[256];
            std::string body;
            std::string status_code;
            if (http_method == "GET"); {
                body = "Hello from server\n";
                status_code = "200 OK";
            }
            if (http_method == "POST") {
                body = "You're not allowed to watch this\n";
                status_code = "403 Forbidden";
            }
            sprintf(send_msg,
                "HTTP/1.1 %s\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s", status_code.c_str(), int(body.size()), body.c_str());

            int rv = send(client_fd, send_msg, strlen(send_msg), 0);
            if (rv == -1) {
                perror("send");
            }
        } else {
            pthread_mutex_unlock(&mutex);
            usleep(100000);
        }
    }
    close(client_fd);
    printf("поток обработки закончил работу\n");
}

void func3() {
    printf("поток ожидания соединений начал работу\n");
    while (flag_wait == 0) {
        socklen_t len = sizeof(addr);
        int client_fd = accept(listen_sock, (struct sockaddr*)&addr, &len);
        if (client_fd == -1) {
            sleep(1);
        } else {
            int* client_fd1 = (int*)malloc(sizeof(int));
            int* client_fd2 = (int*)malloc(sizeof(int));
            *client_fd1 = client_fd;
            *client_fd2 = client_fd;

            t1 = new std::thread(func1, client_fd1);
            t2 = new std::thread(func2, client_fd2);
        }
    }
    printf("поток ожидания соединений закончил работу\n");
}

int main() {
    printf("программа сервера начала работу\n");
    pthread_mutex_init(&mutex, NULL);

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(listen_sock, F_SETFL, O_NONBLOCK);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    int rv = bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr));
    if (rv == -1) {
        perror("bind");
    }

    int optval = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
    &optval, sizeof(optval));

    listen(listen_sock, 100);


    std::thread t3(func3);
    printf("программа ждет нажатия клавиши\n");
    getchar();
    printf("клавиша нажата\n");
    flag_rcv = 1;
    flag_process = 1;
    flag_wait = 1;

    if (t1 && t1->joinable()) { 
        t1->join();
        delete t1;
        t1 = nullptr;
    }
    if (t2 && t2->joinable()) { 
        t2->join();
        delete t2;
        t2 = nullptr;
    }

    t3.join();

    close(client_sock);
    close(listen_sock);

    printf("программа сервера закончила работу\n");
}
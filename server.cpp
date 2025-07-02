#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <ctype.h>
#include <netdb.h>
#include <queue>
#include <string>


int flag_rcv = 0;
int flag_process = 0;
int flag_wait = 0;
int client_sock;
int listen_sock;
struct sockaddr_in addr;
struct sockaddr_in addr2;
pthread_mutex_t mutex;
pthread_t id1, id2, id3;
std::queue<std::string> q;

void* func1(void* arg) {
    int client_fd = *((int*)arg);
    free(((int*)arg)); // Free dynamically allocated memory

    printf("поток приема начал работу\n");
    while (flag_rcv == 0) {
        char rcv_msg[50];
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
    return NULL;
}

void* func2(void* arg) {
    int client_fd = *((int*)arg);
    printf("поток обработки начал работу\n");

    while (flag_process == 0) {
        pthread_mutex_lock(&mutex);
        if (!q.empty()) {
            std::string first_ent = q.front();
            q.pop();
            pthread_mutex_unlock(&mutex);

            printf("Сообщение %s принято\n", first_ent);

            char send_msg[256];
            sprintf(send_msg,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 18\r\n"
                "\r\n"
                "Hello from server\n");

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
    return NULL;
}

void* func3(void*) {
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

            pthread_create(&id1, NULL, func1, client_fd1);
            pthread_create(&id2, NULL, func2, client_fd2);
        }
    }
    printf("поток ожидания соединений закончил работу\n");
    return NULL;
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

    pthread_create(&id3, NULL, func3, NULL);
    printf("программа ждет нажатия клавиши\n");
    getchar();
    printf("клавиша нажата\n");
    flag_rcv = 1;
    flag_process = 1;
    flag_wait = 1;
    pthread_join(id1, NULL);
    pthread_join(id2, NULL);
    pthread_join(id3, NULL);

    close(client_sock);
    close(listen_sock);

    printf("программа сервера закончила работу\n");
}
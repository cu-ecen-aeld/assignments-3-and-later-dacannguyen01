#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define PORT "9000"
#define BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"

int server_socket = -1;
int client_socket = -1;

void handle_signal(int sig) {
    printf("Caught signal, exiting\n");
    if (server_socket != -1) close(server_socket);
    if (client_socket != -1) close(client_socket);
    remove(FILE_PATH);
    exit(0);
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    umask(0);
    if (setsid() < 0) {
        perror("Failed to create new session");
        exit(EXIT_FAILURE);
    }
    if (chdir("/") < 0) {
        perror("Failed to change directory to root");
        exit(EXIT_FAILURE);
    }
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main(int argc, char *argv[]) {
    struct addrinfo hints, *res;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int daemon_mode = 0;
    int optval = 1;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            daemon_mode = 1;
        }
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
        perror("getaddrinfo failed");
        return -1;
    }

    if ((server_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
        perror("Failed to create socket");
        freeaddrinfo(res);
        return -1;
    }

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("Failed to set socket options");
        close(server_socket);
        freeaddrinfo(res);
        return -1;
    }

    if (bind(server_socket, res->ai_addr, res->ai_addrlen) == -1) {
        perror("Failed to bind socket");
        close(server_socket);
        freeaddrinfo(res);
        return -1;
    }
    
    freeaddrinfo(res);

    if (daemon_mode) {
        daemonize();
    }

    if (listen(server_socket, 10) == -1) {
        perror("Failed to listen on socket");
        return -1;
    }

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("Failed to accept connection");
            continue;
        }

        printf("Accepted connection from %s\n", inet_ntoa(client_addr.sin_addr));

        int file_fd = open(FILE_PATH, O_CREAT | O_WRONLY | O_APPEND, 0666);
        if (file_fd == -1) {
            perror("Failed to open file");
            close(client_socket);
            continue;
        }

        ssize_t bytes_received;
        while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
            buffer[bytes_received] = '\0';
            write(file_fd, buffer, bytes_received);
            if (strchr(buffer, '\n')) break;
        }
        close(file_fd);

        file_fd = open(FILE_PATH, O_RDONLY);
        if (file_fd != -1) {
            while ((bytes_received = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
                send(client_socket, buffer, bytes_received, 0);
            }
            close(file_fd);
        }

        printf("Closed connection from %s\n", inet_ntoa(client_addr.sin_addr));
        close(client_socket);
    }

    return 0;
}


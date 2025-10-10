#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#include "server.h"

const unsigned char IAC_IP[3] = "\xff\xf4";

// Section Request
int handle_read(int fd, request* reqP) {
    /*  Return value:
     *     -1: read failed
     *      0: EOF (client down)
     *      1: read successfully
     */
    int r;
    char buf[BUFFER_SIZE];
    size_t len;

    memset(buf, 0, sizeof(buf));

    // Read in request from client
    r = read(fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
    char* p1 = strstr(buf, "\015\012"); // \r\n
    if (p1 == NULL) {
        p1 = strstr(buf, "\012");   // \n
        if (p1 == NULL) {
            if (!strncmp(buf, IAC_IP, 2)) {
                // Client presses ctrl+C, regard as disconnection
                // fprintf(stderr, "Client presses ctrl+C....\n");
                return 0;
            }
        }
    }

    len = min(p1 - buf, BUFFER_SIZE - reqP->length - 1); // with padding
    memmove(reqP->buffer + reqP->length, buf, len);
    reqP->length += len;
    reqP->buffer[reqP->length] = '\0';
    return 1;
}

void reset_request(request * reqP) {
    reqP->length = 0;
    reqP->buffer[0] = '\0';
}

int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    int listen_fd, new_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);
    unsigned short port = (unsigned short) atoi(argv[1]);
    char buffer[BUFFER_SIZE];

    // Initialize server socket
    listen_fd = init_server(port);
    if (listen_fd < 0) {
        fprintf(stderr, "Failed to initialize server.\n");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);

    // Create one client
    request req;
    reset_request(&req);

    // Blocking accept
    new_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addrlen);
    if (new_fd == -1) {
        perror("accept");
    }

    while (1) {
        // Blocking read
        int ret = handle_read(new_fd, &req);
        if (ret > 0) {
            printf("read from client: %s\n", req.buffer);

            // parse command
            if (req.buffer[0] == ' ') {
                write(new_fd, "Invalid command", 15);
                reset_request(&req);
                continue;
            }

            // find read
            if (strlen(req.buffer) >= 6 && strncmp(req.buffer, "read ", 5) == 0) {
                if (req.buffer[5] == ' ') {
                    write(new_fd, "Invalid command", 15);
                    reset_request(&req);
                    continue;
                }

                bool flag = false;
                char* idx_str = req.buffer + 5;
                for (int i = 0; idx_str[i] != '\0' && !flag; i++) {
                    if (!isdigit(idx_str[i])) {
                        flag = true;
                        break;
                    }
                }
                if (flag == true) {
                    write(new_fd, "Invalid command", 15);
                    reset_request(&req);
                    continue;
                }

                int idx = atoi(idx_str);
                handle_read_command(new_fd, idx);
            }
            // find update
            else if (strlen(req.buffer) >= 10 && strncmp(req.buffer, "update ", 7) == 0) {
                if (req.buffer[7] == ' ') {
                    write(new_fd, "Invalid command", 15);
                    reset_request(&req);
                    continue;
                }
                
                char* idx_str = req.buffer + 7;
                bool flag = false;
                int i;
                for (i = 0; idx_str[i] != '\0' && idx_str[i] != ' ' && !flag; i++) {
                    if (!isdigit(idx_str[i])) {
                        flag = true;
                        break;
                    }
                }

                char* content = &idx_str[i + 1];
                if (flag == true || idx_str[i] == '\0' || content[0] == '\0') {
                    write(new_fd, "Invalid command", 15);
                    reset_request(&req);
                    continue;
                }
                idx_str[i] = '\0';
                
                int idx = atoi(idx_str);
                handle_update_command(new_fd, idx, content);
            }
            else if (strcmp(req.buffer, "exit") == 0) {
                close(new_fd);
                break;
            }
            else {
                write(new_fd, "Invalid command", 15);
            }

            reset_request(&req);
        } else {
            // Close connection
            printf("client disconnected\n");
            close(new_fd);
            break;
        }
    }

    return 0;
}

// Server
int init_server(unsigned short port) {
    int listen_fd;
    struct sockaddr_in server_addr;

    // Create listening socket
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    // Allow reuse of address
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(listen_fd);
        return -1;
    }

    // Bind to port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(listen_fd);
        return -1;
    }

    // Start listening
    if (listen(listen_fd, BACKLOG) == -1) {
        perror("listen");
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

void handle_read_command(int client_fd, int idx) {
    int idx_fd = open("index", O_RDONLY);
    int note_fd = open("note.txt", O_RDONLY);
    if (idx_fd < 0 || note_fd < 0) {
        write(client_fd, "Invalid command", 15);
        return;
    }

    unsigned char lengths[256];
    int n = read(idx_fd, lengths, sizeof(lengths));
    char test_buffer[100];
    if (idx < 0 || idx >= n) {
        write(client_fd, "Invalid command", 15);
        close(idx_fd);
        close(note_fd);
        return;
    }

    // 計算該段落的起始位移
    off_t offset = 0;
    for (int i = 0; i < idx; i++) {
        offset += lengths[i];
    }

    // 讀取該段內容
    char buf[MESSAGE_SIZE + 1];
    lseek(note_fd, offset, SEEK_SET);
    int len = lengths[idx];
    read(note_fd, buf, len);
    buf[len] = '\0';

    // 回傳給 client
    write(client_fd, buf, len);
    // write(client_fd, "\n", 1);

    close(idx_fd);
    close(note_fd);
}


void handle_update_command(int client_fd, int idx, const char *content) {
    int idx_fd = open("index", O_RDWR);
    int note_fd = open("note.txt", O_RDWR);
    if (idx_fd < 0 || note_fd < 0) {
        write(client_fd, "Invalid command", 15);
        return;
    }

    unsigned char lengths[256];
    int n = read(idx_fd, lengths, sizeof(lengths));

    if (idx < 0 || idx >= n || content[0] == '\0') {
        write(client_fd, "Invalid command", 15);
        close(idx_fd);
        close(note_fd);
        return;
    }

    // 計算偏移量
    off_t offset = 0;
    for (int i = 0; i < idx; i++)
        offset += lengths[i];

    // 讀出整個 note.txt
    char note_buf[MESSAGE_SIZE * MESSAGE_SIZE + 1];
    lseek(note_fd, 0, SEEK_SET);
    int total = read(note_fd, note_buf, sizeof(note_buf));

    int old_len = lengths[idx];
    int new_len = strlen(content);
    if (new_len > MESSAGE_SIZE) new_len = MESSAGE_SIZE;

    // shift 後面內容
    memmove(note_buf + offset + new_len, note_buf + offset + old_len, total - offset - old_len);

    // 插入新內容
    memcpy(note_buf + offset, content, new_len);

    // 更新 index
    lengths[idx] = new_len;

    // 重寫 note.txt
    ftruncate(note_fd, 0);
    lseek(note_fd, 0, SEEK_SET);
    write(note_fd, note_buf, total - old_len + new_len);

    // 重寫 index
    ftruncate(idx_fd, 0);
    lseek(idx_fd, 0, SEEK_SET);
    write(idx_fd, lengths, n);

    close(note_fd);
    close(idx_fd);
}
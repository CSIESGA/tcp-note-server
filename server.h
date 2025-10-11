#ifndef __SERVER_H
#define __SERVER_H

#define BACKLOG 10
#define MAX_CLIENTS 100
#define BUFFER_SIZE 256
#define MESSAGE_SIZE 200
#define min(a, b) ((a) < (b) ? (a) : (b))
#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

// Section Request
typedef struct {
    size_t length;
    char buffer[BUFFER_SIZE];
} request;

int handle_read(int fd, request* reqP);
void reset_request(request * reqP);
void process_command(int new_fd, request* req, struct pollfd* fds);
void handle_read_command(int client_fd, int idx);
void handle_update_command(int client_fd, int idx, const char *content);
void lock_region(int fd, short type, off_t start, off_t len);
void unlock_region(int fd, off_t start, off_t len);
void lock_read_range(int fd, off_t start, off_t len);
void lock_write_range(int fd, off_t start, off_t len);
void unlock_range(int fd, off_t start, off_t len);

// Section Server
int init_server(unsigned short port);

#endif
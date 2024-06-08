#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>     //for sockaddr_in
#include <unistd.h>         //for write, read
#include <string.h>         //for strlen
#include <stdlib.h>         //for atol
#include <sys/sendfile.h>   //for sendfile
#include <fcntl.h>          //for open
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <poll.h>
#include <pthread.h>


char* headers = "HTTP/1.1 %d %s\nContent-Type: text/html; charset=UTF-8\ncontent-length: %d\n\n";
void* handle_connection(void* p_conn);

const char* parse(char* request) {
    const char* method = strtok(request, " ");
    const char* path = strtok(NULL, " /");
    // const char* version = strtok(NULL, " ");
    // if (method && path && version) {
    //     fprintf(stderr, "Method: %s\nPath: %s\nVersion: %s\n\n", method, path, version);
    // }

    if (!path)
        return NULL;
    return path;
}

void write_headers(int sock, int code, const char* status, size_t length) {
    char buf[4096];
    sprintf(buf, headers, code, status, length);
    write(sock, buf, strlen(buf));
}

//usage: gcc http_server.c -o http
//./http 0 8080
//in other terminal: wget 0:8080/file.html

int main(int argc, char** argv) {
    if(argc != 3) {
        fprintf(stderr, "Error, incorrect agruments\n");
        exit(1);
    }
    
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock == -1) {
        fprintf(stderr, "Error, failed to create socket\n");
        exit(1);
    }

    short port = atol(argv[2]);
	struct hostent* h = gethostbyname(argv[1]);
	struct in_addr address = {
		.s_addr = *((uint32_t*)h->h_addr_list[0])
	};
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr = address
	};
    
    if(bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(server_sock);
        fprintf(stderr, "Error, failed to bind socket\n");
        exit(1);
    }

    if(listen(server_sock, 15) == -1) {
        close(server_sock);
        fprintf(stderr, "Error, failed to listen on socket\n");
        exit(1);
    }

	printf("Accepting\n");
    while(1) {
        int client_sock = accept(server_sock, NULL, 0);
        if(client_sock == -1) {
            fprintf(stderr, "Failed to accept client sock\n");
            continue;
        }

	printf("Creating thread\n");
 	pthread_t t;
	int* client_socket = (int*)malloc(sizeof(int));
	*client_socket = client_sock;
 	pthread_create(&t, NULL, handle_connection, (void*)(client_socket)); 
    }
    close(server_sock);
}



void* handle_connection(void* p_conn){

	printf("Opening file\n");
	int client_sock = *(int*)(p_conn);
       FILE* fconn = fdopen(client_sock, "r");
        char* request = NULL;
        size_t n = 0;
        if (getline(&request, &n, fconn) == -1) {
            free(request);
	    fprintf(stderr, "Request read failed\n");
            close(client_sock);
            return NULL;
        }

        //polling client sock(waiting for more inputs, in our case just skipping HTTP headers)
        struct pollfd pfds[] = {{ .fd = client_sock, .events = POLLIN }};
		while (poll(pfds, 1, 0) == 1) {
			char buf[128] = {0};
			int n = read(client_sock, buf, sizeof(buf));
        }

        const char* file_path = parse(request);
        printf("File path: %s\n", file_path);

        if(file_path == NULL) {
            free(request);
            char* resp = "Bad request";
            write_headers(client_sock, 400, resp, sizeof(resp));
            close(client_sock);
		return NULL;
        }

        int opened_fd = open(file_path, O_RDONLY);
        free(request);
        if(opened_fd == -1) {
            char* resp = "Not found";
            write_headers(client_sock, 404, resp, sizeof(resp));
            close(client_sock);
		return NULL;
        }

        struct stat sb;
        fstat(opened_fd, &sb);
        write_headers(client_sock, 200, "OK", sb.st_size);
        sendfile(client_sock, opened_fd, 0, sb.st_size);
        close(client_sock);
}

/*
 * Starter code for proxy lab.
 * Feel free to modify this code in whatever way you wish.
 * @author Qinlin Jia <qinlinj@andrew.cmu.edu>
 */

#include "csapp.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>

/*
 * Debug macros, which can be enabled by adding -DDEBUG in the Makefile
 * Use these if you find them useful, or delete them if not
 */
#ifdef DEBUG
#ifdef dbg_assert
#undef dbg_assert
#endif
#define dbg_assert(...) assert(__VA_ARGS__)

#ifdef dbg_printf
#undef dbg_printf
#endif
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#ifdef dbg_assert
#undef dbg_assert
#endif
#define dbg_assert(...)

#ifdef dbg_printf
#undef dbg_printf
#endif
#define dbg_printf(...)
#endif

/*
 * Max cache and object sizes
 * You might want to move these to the file containing your cache implementation
 */
#define MAX_CACHE_SIZE (1024 * 1024)
#define MAX_OBJECT_SIZE (100 * 1024)

void doit(int fd);
void client_error(int fd, char *cause, char *errnum, char *shortmsg,
                  char *longmsg);
int parse_url(char *url, char *port, char *servername, char *filename);
int forward_request(rio_t *rio, char *servername, char *port, char *filename);
int forward_response(int client_fd, int server_fd);
void *thread(void *arg);
/*
 * String to use for the User-Agent header.
 * Don't forget to terminate with \r\n
 */
// static const char *header_user_agent = "Mozilla/5.0"
//                                        " (X11; Linux x86_64; rv:3.10.0)"
//                                        " Gecko/20220411 Firefox/63.0.1";

typedef struct sockaddr SA;

int main(int argc, char **argv) {
    int listen_fd, client_fd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t client_len;
    struct sockaddr_storage client_addr;
    pthread_t tid;
    // Ignore SIGPIPE to handle write errors on socket
    signal(SIGPIPE, SIG_IGN);

    // Check for correct usage
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // Open a listening socket
    listen_fd = open_listenfd(argv[1]);
    if (listen_fd < 0) {
        fprintf(stderr, "Error: unable to open listening socket on port %s\n",
                argv[1]);
        exit(1);
    }

    // Main loop: accept and handle requests
    while (1) {
        client_len = sizeof(client_addr);
        client_fd = accept(listen_fd, (SA *)&client_addr, &client_len);

        // Check for errors in accept
        if (client_fd < 0) {
            fprintf(stderr, "Error: failed to accept connection\n");
            continue; // Skip to next iteration
        }

        // Get client information
        if (getnameinfo((SA *)&client_addr, client_len, hostname, MAXLINE, port,
                        MAXLINE, 0) != 0) {
            fprintf(stderr, "Error: failed to get client information\n");
            close(client_fd);
            continue; // Skip to next iteration
        }

        printf("Accepted connection from (%s, %s)\n", hostname, port);

        // // Handle the client's request
        // doit(client_fd);

        // // Close the client socket
        // close(client_fd);
        pthread_create(&tid, NULL, thread, (void *)(intptr_t)client_fd);
    }

    return 0;
}

/**
 * Handles a single client request.
 *
 * @param client_fd File descriptor for the client connection.
 */
void doit(int client_fd) {
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], servername[MAXLINE], port[6];
    rio_t client_rio;
    int server_fd, parse_result;

    // Initialize the client read buffer
    rio_readinitb(&client_rio, client_fd);

    // Read the request line from the client
    if (!rio_readlineb(&client_rio, buf, MAXLINE)) {
        fprintf(stderr, "Error reading request line from client.\n");
        return;
    }

    // Parse the request line
    if (sscanf(buf, "%s %s %s", method, url, version) < 3) {
        fprintf(stderr, "Error parsing request line: %s\n", buf);
        client_error(client_fd, "Parsing Error", "400", "Bad request",
                     "Cannot parse the request line");
        return;
    }

    // Only support the GET method
    if (strcasecmp(method, "GET") != 0) {
        client_error(client_fd, method, "501", "Not implemented",
                     "This proxy only supports the GET method");
        return;
    }

    // Extract hostname, port, and filename from URL
    parse_result = parse_url(url, port, servername, filename);
    if (parse_result != 0) {
        fprintf(stderr, "Error parsing URL: %s\n", url);
        client_error(client_fd, url, "400", "Bad request",
                     "Cannot parse the URL");
        return;
    }

    // Attempt to forward the request to the server
    server_fd = forward_request(&client_rio, servername, port, filename);
    if (server_fd < 0) {
        fprintf(stderr, "Error connecting to server: %s:%s\n", servername,
                port);
        client_error(client_fd, servername, "500", "Internal server error",
                     "Error forwarding the request");
        return;
    }

    // Forward the response from the server back to the client
    forward_response(client_fd, server_fd);

    // Clean-up
    close(server_fd);
}

/**
 * Parses the URL into servername, port, and filename.
 *
 * @param url The URL to parse.
 * @param port The buffer to store the parsed port number.
 * @param servername The buffer to store the parsed server name.
 * @param filename The buffer to store the parsed file name.
 * @return 0 on success, -1 on failure.
 */
int parse_url(char *url, char *port, char *servername, char *filename) {
    char *host_start, *host_end, *path_start;

    // Ensure arguments are not NULL
    if (!url || !port || !servername || !filename) {
        return -1;
    }

    // Default port is 80
    strcpy(port, "80");

    // Find the beginning of the hostname
    host_start = strstr(url, "//");
    host_start = host_start ? host_start + 2 : url;

    // Find the start of the path and end of the hostname
    path_start = strchr(host_start, '/');
    if (path_start) {
        strcpy(filename, path_start); // Copy the path to filename
        *path_start = '\0';           // Null-terminate the hostname
    } else {
        filename[0] = '\0'; // If no path is found, filename is empty
    }

    // Find the port number if it exists
    host_end = strchr(host_start, ':');
    if (host_end) {
        *host_end = '\0';           // Null-terminate the hostname
        strcpy(port, host_end + 1); // Copy the port number
    }

    // Copy the hostname to servername
    strcpy(servername, host_start);

    return 0;
}

/**
 * Sends an HTTP error response to the client.
 *
 * @param fd The file descriptor to write the error message to.
 * @param cause The cause of the error.
 * @param errnum The HTTP error number (status code).
 * @param shortmsg A short description of the error.
 * @param longmsg A longer explanation of the error.
 */
void client_error(int fd, char *cause, char *errnum, char *shortmsg,
                  char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    // Build the HTTP response body
    snprintf(body, MAXBUF,
             "<html><title>Proxy Error</title>"
             "<body bgcolor=\"ffffff\">"
             "%s: %s<p>%s: %s"
             "<hr><em>The Proxy Server</em>"
             "</body></html>",
             errnum, shortmsg, longmsg, cause);

    // Write the HTTP response headers
    snprintf(buf, MAXLINE, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    snprintf(buf, MAXLINE, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    snprintf(buf, MAXLINE, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));

    // Write the response body
    rio_writen(fd, body, strlen(body));
}

/**
 * Forwards the client's request to the server.
 *
 * @param read_rio The read buffer for the client's request.
 * @param servername The server's hostname.
 * @param port The server's port number.
 * @param filename The requested filename/path.
 * @return The server's file descriptor on success, -1 on error.
 */
int forward_request(rio_t *read_rio, char *servername, char *port,
                    char *filename) {
    char buf[MAXLINE], request_hdr[MAXLINE], other_hdrs[MAXLINE] = "";
    int server_fd;

    server_fd = open_clientfd(servername, port);
    if (server_fd < 0)
        return -1;

    // Construct and send the request line
    snprintf(request_hdr, MAXLINE, "GET %s HTTP/1.0\r\n", filename);
    rio_writen(server_fd, request_hdr, strlen(request_hdr));

    // Construct and send the necessary headers
    snprintf(other_hdrs, MAXLINE,
             "Host: %s:%s\r\n"
             "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:3.10.0) "
             "Gecko/20220411 Firefox/63.0.1\r\n"
             "Connection: close\r\n"
             "Proxy-Connection: close\r\n",
             servername, port);
    rio_writen(server_fd, other_hdrs, strlen(other_hdrs));

    // Forward other necessary headers from the client
    while (rio_readlineb(read_rio, buf, MAXLINE) > 0) {
        if (strcmp(buf, "\r\n") == 0) {
            rio_writen(server_fd, buf, strlen(buf));
            break; // End of headers
        }
        if (!strstr(buf, "Host:") && !strstr(buf, "User-Agent:") &&
            !strstr(buf, "Connection:") && !strstr(buf, "Proxy-Connection:")) {
            rio_writen(server_fd, buf, strlen(buf));
        }
    }

    return server_fd;
}

/**
 * Forwards the server's response back to the client.
 *
 * @param client_fd The client's file descriptor.
 * @param server_fd The server's file descriptor.
 * @return 0 on successful forwarding, -1 on error.
 */
int forward_response(int client_fd, int server_fd) {
    char buf[MAXLINE];
    rio_t server_rio;
    ssize_t num;

    // Initialize the read buffer for the server's response
    rio_readinitb(&server_rio, server_fd);

    // Read from server and write to client
    while ((num = rio_readnb(&server_rio, buf, MAXLINE)) > 0) {
        if (rio_writen(client_fd, buf, num) != num)
            return -1; // Write error
    }

    // Check for read error
    if (num < 0)
        return -1;

    return 0; // Success
}

/**
 * Handles a client connection in a separate thread.
 *
 * This function detaches the current thread for independent execution,
 * calls a function 'doit' to process the client's request, and then
 * closes the client's file descriptor.
 *
 * @param client_fd The client's file descriptor.
 * @return NULL after closing the client's connection.
 */
void *thread(void *arg) {
    int client_fd = (int)(intptr_t)arg;
    pthread_detach(pthread_self());
    doit(client_fd);
    close(client_fd);
    return NULL;
}
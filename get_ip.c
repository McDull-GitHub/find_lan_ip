#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8964
#define VERSION "1.0"

static char *progname;
static int sock;

static void usage(void)
{
    fprintf(stderr,
    "Version: %s\n"
    "Usage: %s [-s|-c] name\n"
    "\t-s\tRun as server\n"
    "\t-c\tRun as client\n",
    VERSION,progname);
    exit(1);
}

static int udp_socket(void)
{
    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        fprintf(stderr, "Cannot open a new socket: %s\n", strerror(errno));
        exit(1);
    }
    return sock;
}

static void server(const char *name)
{
    struct sockaddr_in listen_addr;

    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(PORT);

    if (bind(sock, (struct sockaddr *) &listen_addr,
             sizeof(listen_addr)) == -1)
    {
        fprintf(stderr, "Cannot bind to the UDP port %d: %s\n",
                PORT, strerror(errno));
        exit(1);
    }

    while (1) {
        char buf[BUFSIZ+1];
        struct sockaddr_in addr;
        int addrlen = sizeof(addr);
        int received;

        received = recvfrom(sock, buf, sizeof(buf)-1, 0,
                            (struct sockaddr *) &addr, (socklen_t *) &addrlen);
        if (received == -1)
            continue;

        buf[received] = 0;
        if (strcmp(buf, name) == 0)
            sendto(sock, "bingo", 5, 0, (struct sockaddr *) &addr, addrlen);
    }
}

static void *waiton_response(void *arg)
{
    char *name = (char *) arg;

    while (1) {
        char buf[BUFSIZ+1];
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        int received;

        received = recvfrom(sock, buf, sizeof(buf)-1, 0,
                            (struct sockaddr *) &addr, &addrlen);
        if (received == -1)
            continue;
        buf[received] = 0;
        if (strcmp(buf, "bingo") == 0) {
            printf("%s's address is %s.\n",
                   name, inet_ntoa(addr.sin_addr));
            exit(0);
        }
    }

    return NULL;
}

static void client(const char *name)
{
    int bcast = 1;
    pthread_t tid;
    pthread_attr_t attr;
    struct sockaddr_in addr;
    int i;

    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast));

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = 0xffffffff;
    addr.sin_port = htons(PORT);

    for (i = 0; i < 6; i++) {
        printf("Searching...\n");

        sendto(sock, name, strlen(name), 0,
               (struct sockaddr *) &addr, sizeof(addr));
        if (i == 0)
            pthread_create(&tid, &attr, waiton_response, (void *) name);
        sleep(5);
    }

    printf("Cannot find the machine.\n");
    pthread_attr_destroy(&attr);
}

int main(int argc, char **argv)
{
    progname = argv[0];
    sock = udp_socket();

    if (argc != 3)
        usage();
    if (strcmp(argv[1], "-s") == 0)
        server(argv[2]);
    else if (strcmp(argv[1], "-c") == 0)
        client(argv[2]);
    else
        usage();

    return 0;
}

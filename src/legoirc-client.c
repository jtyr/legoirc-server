#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>


// Max number of bytes we can get at once
#define MAXDATASIZE 100


// FROM: http://c-faq.com/osdep/cbreak.html
static struct termio saved_modes;
static int have_modes = 0;

int tty_break() {
    struct termio modmodes;

    if(ioctl(fileno(stdin), TCGETA, &saved_modes) == -1) {
        return -1;
    }

    have_modes = 1;

    modmodes = saved_modes;
    modmodes.c_lflag &= ~(ICANON | ECHO);
    modmodes.c_cc[VMIN] = 1;
    modmodes.c_cc[VTIME] = 0;

    return ioctl(fileno(stdin), TCSETAW, &modmodes);
}

int tty_fix() {
    if(! have_modes) {
        return 0;
    }

    return ioctl(fileno(stdin), TCSETAW, &saved_modes);
}


void int_handler() {
    // Restore the original console I/O modes
    if (tty_fix() == -1) {
        perror("ERROR on restoring tty");
        exit(EXIT_FAILURE);
    }

    puts("Interuption caught");

    exit(EXIT_SUCCESS);
}


// Get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    } else if (sa->sa_family == AF_INET6) {
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    } else {
        perror("ERROR address family");
        exit(EXIT_FAILURE);
    }
}


void usage(char *name) {
    printf("Usage: %s [options]\n\n", name);
    puts("Options:");
    puts(" -s STR  Server IP");
    puts(" -p NUM  Server port number (default: 5001)");
    puts(" -h      Show this help message and exit");
}


int main(int argc, char *argv[]) {
    struct sockaddr_in server;
    char str[2] = "\n\n";
    char *host = NULL;
    int port = 5001;
    int sock, keycode, c;

    // Parse command line options
    while ((c = getopt(argc, argv, "s:p:h")) != -1) {
        switch (c) {
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            case 's':
                host = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            default:
                abort();
        }
    }

    // Check if server IP is defined
    if (host == NULL) {
        puts("ERROR: Server IP not specified.\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Connecting to %s:%d\n", host, port);

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("ERROR opening socket");
        exit(EXIT_FAILURE);
    }

    // Initialize socket
    bzero((char *) &server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(host);
    server.sin_port = htons(port);

    // Catch interuption signal
    if (signal(SIGINT, int_handler) == SIG_ERR) {
        perror("ERROR on setting signal");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *) &server, sizeof(server)) == -1) {
        perror("ERROR on connect");
        exit(EXIT_FAILURE);
    }

    // Disable new line and echo while reading keys
    if (tty_break() == -1) {
        perror("ERROR on tty break");
        exit(EXIT_FAILURE);
    }

    puts("Quit by pressing 'q' key.");

    // Read keys in infinite loop (untill pressed "q" or CRTL+C)
    while (1) {
        // Read code from the keyboard
        keycode = getchar();

        if (keycode == 'q') {
            // Last message is newline
            str[0] = '\n';
        } else {
            // Convert the keycode to a character of the string
            str[0] = keycode;
        }

        // Write the string to the socket
        if (write(sock, str, strlen(str)) == -1) {
            perror("ERROR on writing to socket");
            exit(EXIT_FAILURE);
        }

        // Finish when pressed "q"
        if (keycode == 'q') {
            puts("Closing connection");
            break;
        }
    }

    // Restore the original console I/O modes
    if (tty_fix() == -1) {
        perror("ERROR on restoring tty");
        exit(EXIT_FAILURE);
    }

    // Close the socket
    if (close(sock) == -1) {
        perror("ERROR on close");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

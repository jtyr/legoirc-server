#include <bcm2835.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>


// Max length of queue for the incomming connections
#define BACKLOG 10

// Max length of incomming message
#define BUFSIZE 100

// IR modes
#define MODE_EXTENDED 1
#define MODE_COMBO_DIRECT 2
#define MODE_SINGLE_OUTPUT 3
#define MODE_COMBO_PWM 4

// Direction keycodes
#define KEYCODE_BACKWARD_LEFT 49
#define KEYCODE_BACKWARD 50
#define KEYCODE_BACKWARD_RIGHT 51
#define KEYCODE_LEFT 52
#define KEYCODE_STOP 53
#define KEYCODE_RIGHT 54
#define KEYCODE_FORWARD_LEFT 55
#define KEYCODE_FORWARD 56
#define KEYCODE_FORWARD_RIGHT 57

// Max length of the message in the shared memory
#define SHM_MSG_SIZE 3

// Waiting time in the send command loop
#define CMD_LOOP_WAIT 1e5

// Debug variable
int DEBUG = 0;

// Pin to which the data cable is connected (GPIO24)
int GPIO_PIN = RPI_BPLUS_GPIO_J8_18;

// IR channel
int CHANNEL = 1;

// 0xf vector needed for the XOR checksum
int F_VECT[4]  = {1, 1, 1, 1};

// IR protocol parts (http://powerfunctions.lego.com/en-GB/ElementSpecs/8884.aspx)
int NIBBLE1[4] = {0, 1, 0, 0};
// Left & right
int NIBBLE2[4] = {0, 0, 0, 0};
// Forward & backward
int NIBBLE3[4] = {0, 0, 0, 0};

// Directions for the Combo PWM mode (using step 7)
int MODE4_DIR_NULL[4]     = {0, 0, 0, 0};
int MODE4_DIR_STOP[4]     = {1, 0, 0, 0};
int MODE4_DIR_FORWARD[4]  = {0, 1, 1, 1};
int MODE4_DIR_BACKWARD[4] = {1, 0, 0, 1};
int MODE4_DIR_LEFT[4]     = {0, 1, 1, 1};
int MODE4_DIR_RIGHT[4]    = {1, 0, 0, 1};

// IR mode to be used
int MODE = 4;

// Variables initialized in the init() function
float PULSE_LEN, LOW_BIT_WAIT, HIGH_BIT_WAIT, START_BIT_WAIT, STOP_BIT_WAIT;
float MAX_MSG_LEN, CHANNEL_WAIT_1, CHANNEL_WAIT_2_3, CHANNEL_WAIT_4_5, MSG_FREQ;

// Shared memory ID
int shm_id;

// Structure used in the IPC
struct record {
    char msg[SHM_MSG_SIZE];
    struct timeval time;
};


void init() {
    // IR frequency (converted to microseconds)
    float FREQ = (float) 1/38 * 1000;

    // LED pulse length
    PULSE_LEN = 6 * FREQ;

    // Bit waiting time (bit length is PULSE_LEN + *_BIT_WAIT)
    LOW_BIT_WAIT = 10 * FREQ;
    HIGH_BIT_WAIT = 21 * FREQ;
    START_BIT_WAIT = 39 * FREQ;
    STOP_BIT_WAIT = START_BIT_WAIT;

    // Max message length (in microseconds)
    MAX_MSG_LEN = (float) 16000;

    // Waiting time between 5 identical messages
    CHANNEL_WAIT_1   = (4 - CHANNEL) * MAX_MSG_LEN;
    CHANNEL_WAIT_2_3 = 5 * MAX_MSG_LEN;
    CHANNEL_WAIT_4_5 = (6 + 2 * CHANNEL) * MAX_MSG_LEN;

    // How ofter we can expect to receive a message
    MSG_FREQ = CHANNEL_WAIT_1 + 2*CHANNEL_WAIT_2_3 + 2*CHANNEL_WAIT_4_5;
}


float get_time_diff(struct timeval *from, struct timeval *to) {
    return (to->tv_sec - from->tv_sec) * 1e6 + (to->tv_usec - from->tv_usec);
}


void led_pulse(float *pause) {
    if (DEBUG > 2)
        printf(".");

    struct timeval t1, t2;
    float t_diff;

    // Timestamp before the LED went on
    gettimeofday(&t1, NULL);

    // Switch the LED on
    bcm2835_gpio_write(GPIO_PIN, HIGH);

    // Calculate how long it took to switch the LED on
    gettimeofday(&t2, NULL);
    t_diff = get_time_diff(&t1, &t2);

    // Keep the LED on for certain period
    if (PULSE_LEN - t_diff > 0) {
        bcm2835_delayMicroseconds(PULSE_LEN - t_diff);
    }

    // Switch the LED off
    bcm2835_gpio_write(GPIO_PIN, LOW);

    // Pause after the pulse (must be constant)
    bcm2835_delayMicroseconds(*pause);
}


void send_high_bit() {
    if (DEBUG > 2)
        printf("1");

    led_pulse(&HIGH_BIT_WAIT);
}


void send_low_bit() {
    if (DEBUG > 2)
        printf("0");

    led_pulse(&LOW_BIT_WAIT);
}


void send_start_bit() {
    if (DEBUG > 2)
        printf("START");

    led_pulse(&START_BIT_WAIT);
}


void send_stop_bit() {
    if (DEBUG > 2)
        printf("STOP");

    led_pulse(&STOP_BIT_WAIT);

    if (DEBUG > 2)
        printf("\n");
}


void send_msg(int *data, int keycode) {
    int n, i;
    struct record *shm_data;

    // Each message must be sent 5 times
    for (n=0; n<5; n++) {
        // Read the current direction from the shared memory
        shm_data = shmat(shm_id, (void *) 0, 0);
        if (shm_data == (struct record *) -1) {
            perror("ERROR on shmat");
            exit(EXIT_FAILURE);
        }

        // Break the loop if the direction has changed
        if (keycode != shm_data->msg[0]) {
            if (DEBUG > 1)
                puts("D: Breaking the send_msg loop because of a new command.");

            break;
        }

        // Wait between messages (must be constant)
        if (n == 0) {
            if (DEBUG > 2)
                printf("%d. MSG (1)\n", n+1);

            bcm2835_delayMicroseconds(CHANNEL_WAIT_1);
        } else if (n == 1 || n == 2) {
            if (DEBUG > 2)
                printf("%d. MSG (2_3)\n", n+1);

            bcm2835_delayMicroseconds(CHANNEL_WAIT_2_3);
        } else {
            if (DEBUG > 2)
                printf("%d. MSG (4_5)\n", n+1);

            bcm2835_delayMicroseconds(CHANNEL_WAIT_4_5);
        }

        // Send the message (max 16ms long)
        send_start_bit();

        for (i=0; i<16; i++) {
            if (data[i] == 0) {
                send_low_bit();
            } else {
                send_high_bit();
            }
        }

        send_stop_bit();
    }
}


int *xor_array(int *a, int *b) {
    static int ret[4];
    int i;

    for (i=0; i<4; i++) {
        if (a[i] == b[i]) {
            ret[i] = 0;
        } else {
            ret[i] = 1;
        }
    }

    return ret;
}


// TODO
int proto_extended_mode(int keycode) {
    puts("I: Extended mode is not implemented yet.");

    return -1;
}


// TODO
int proto_combo_direct_mode(int keycode) {
    puts("I: Combo direct mode is not implemented yet.");

    return -1;
}


// TODO
int proto_single_output_mode(int keycode) {
    puts("I: Single output mode is not implemented yet.");

    return -1;
}


int proto_combo_pwm_mode(int keycode) {
    int recognized_direction = 0;
    int i;

    // Reset directions
    memcpy(&NIBBLE2, &MODE4_DIR_NULL, sizeof NIBBLE2);
    memcpy(&NIBBLE3, &MODE4_DIR_NULL, sizeof NIBBLE3);

    if (DEBUG > 0)
        printf("DIRECTION: ");

    if (keycode >= KEYCODE_BACKWARD_LEFT && keycode <= KEYCODE_BACKWARD_RIGHT) {
        if (DEBUG > 0)
            printf("BACKWARD ");
        memcpy(&NIBBLE3, &MODE4_DIR_BACKWARD, sizeof NIBBLE3);
        recognized_direction = 1;
    } else if (keycode >= KEYCODE_FORWARD_LEFT && keycode <= KEYCODE_FORWARD_RIGHT) {
        if (DEBUG > 0)
            printf("FORWARD ");
        memcpy(&NIBBLE3, &MODE4_DIR_FORWARD, sizeof NIBBLE3);
        recognized_direction = 1;
    }
    if (keycode == KEYCODE_LEFT || keycode == KEYCODE_BACKWARD_LEFT || keycode == KEYCODE_FORWARD_LEFT) {
        if (DEBUG > 0)
            printf("LEFT");
        memcpy(&NIBBLE2, &MODE4_DIR_LEFT, sizeof NIBBLE2);
    } else if (keycode == KEYCODE_STOP) {
        if (DEBUG > 0)
            printf("STOP");
        memcpy(&NIBBLE2, &MODE4_DIR_STOP, sizeof NIBBLE2);
        memcpy(&NIBBLE3, &MODE4_DIR_STOP, sizeof NIBBLE3);
    } else if (keycode == KEYCODE_RIGHT || keycode == KEYCODE_BACKWARD_RIGHT || keycode == KEYCODE_FORWARD_RIGHT) {
        if (DEBUG > 0)
            printf("RIGHT");
        memcpy(&NIBBLE2, &MODE4_DIR_RIGHT, sizeof NIBBLE2);
    } else if (keycode == 113) {
        if (DEBUG > 0)
            printf("QUIT\n");
        return -1;
    } else if (recognized_direction == 0) {
        if (DEBUG > 0)
            printf("??? (%d)\n", keycode);
        return -1;
    }

    if (DEBUG > 0)
        printf("\n");

    // Calculate the checksum
    int *lrc = xor_array(xor_array(xor_array(F_VECT, NIBBLE1), NIBBLE2), NIBBLE3);

    // Compose the final message
    int data[16];
    for (i=0; i<4; i++) {
        data[i]    = NIBBLE1[i];
        data[i+4]  = NIBBLE2[i];
        data[i+8]  = NIBBLE3[i];
        data[i+12] = lrc[i];
    }

    // Send the message
    send_msg(data, keycode);

    return 0;
}


int socket_readline(int sock, char **line) {
    int buf_size = BUFSIZE;
    int bytesloaded = 0;
    int n;
    char c;
    char *buffer = malloc(buf_size);
    char *newbuf;

    if (buffer == NULL) {
        return -1;
    }

    while (1) {
        // Read a single byte
        if ((n = read(sock, &c, 1)) == -1) {
            free(buffer);
            return -1;
        }

        // Break at the end of the line
        if (c == '\n') {
            buffer[bytesloaded] = '\0';
            break;
        }

        buffer[bytesloaded] = c;
        bytesloaded++;

        // Allocate more memory if needed
        if (bytesloaded + 1 >= buf_size) {
            buf_size += BUFSIZE;
            newbuf = realloc(buffer, buf_size);

            if (newbuf == NULL) {
                free(buffer);
                return -1;
            }

            buffer = newbuf;
        }
    }

    // If the line was terminated by "\r\n", ignore the "\r"
    if (bytesloaded && (buffer[bytesloaded - 1] == '\r')) {
        buffer[bytesloaded - 1] = '\0';
        bytesloaded--;
    }

    // Complete the line
    *line = buffer;

    // Number of bytes in the line
    return bytesloaded;
}


// Read messages from the client
void read_client_msgs(int sock, struct record *shm_data) {
    char *line;
    int n;

    while (1) {
        // Read line from the socket
        n = socket_readline(sock, &line);
        if (n == -1) {
            perror("ERROR reading from socket");
            exit(EXIT_FAILURE);
        }

        // Close connection on empty string
        if (strcmp(line, "X") == 0) {
            if (DEBUG > 0)
                puts("D: Shutting down the server");

            if (execl("/sbin/shutdown", "shutdown", "-h", "now", NULL) == -1) {
                perror("ERROR on exec");
                exit(EXIT_FAILURE);
            }
        } else if (n == 0) {
            if (DEBUG > 0)
                puts("D: Client closed connection");
            break;
        } else {
            if (DEBUG > 1)
                printf("D: Here is the message: >%s<\n", line);

            // Store the line into the shared memory
            strncpy(shm_data->msg, line, SHM_MSG_SIZE);
            gettimeofday(&shm_data->time, NULL);
        }
    }
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
    puts(" -p NUM  Server port number (default: 5001)");
    puts(" -c NUM  IR channel (default: 1)");
    puts(" -m NUM  IR mode");
    puts("           1 = Extended mode");
    puts("           2 = Combo direct mode");
    puts("           3 = Single output mode");
    puts("           4 = Combo PWM mode (default)");
    puts(" -g NUM  GPIO (default: 24)");
    puts(" -d NUM  Debug level [0-3] (default: 0)");
    puts(" -h      Show this help message and exit");
}


int main(int argc, char *argv[]) {
    struct sockaddr_in server, client;
    char ip[INET6_ADDRSTRLEN];
    unsigned int client_len = sizeof(client);
    int yes = 1;
    int port = 5001;
    int sock, new_sock, pid, c;
    struct record *shm_data;

    // Silently reap children
    signal(SIGCHLD, SIG_IGN);

    // Do not buffer STDOUT and STDERR
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // Initiate the global options
    init();

    // Parse command line options
    while ((c = getopt(argc, argv, "g:d:c:m:p:h")) != -1) {
        switch (c) {
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'c':
                CHANNEL = atoi(optarg);
                init();
                break;
            case 'm':
                MODE = atoi(optarg);
                break;
            case 'g':
                GPIO_PIN = atoi(optarg);
                break;
            case 'd':
                DEBUG = atoi(optarg);
                break;
            default:
                abort();
        }
    }

    // Print some information
    if (DEBUG > 0) {
        printf("D: Server port number: %d\n", port);
        printf("D: GPIO: %d\n", GPIO_PIN);
        printf("D: IR channel: %d\n", CHANNEL);
        printf("D: IR mode: %d\n", MODE);
    }

    // Initiate the bus
    if (! bcm2835_init())
        return 1;

    // Set the output pin
    bcm2835_gpio_fsel(GPIO_PIN, BCM2835_GPIO_FSEL_OUTP);

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("ERROR opening socket");
        exit(EXIT_FAILURE);
    }

    // Initialize socket
    bzero((char *) &server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    // Allow to reuse the bind address
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("ERROR on setsockopt");
        exit(EXIT_FAILURE);
    }

    // Bind the host address
    if (bind(sock, (struct sockaddr *) &server, sizeof(server)) == -1) {
        perror("ERROR on binding");
        exit(EXIT_FAILURE);
    }

    // Start listening for the clients
    if (listen(sock, BACKLOG) == -1) {
        perror("ERROR on listening");
        exit(EXIT_FAILURE);
    }

    if (DEBUG > 0)
        puts("D: Server is up");

    // Create the shared memory segment
    if ((shm_id = shmget(IPC_PRIVATE, sizeof(shm_data), 0600 | IPC_CREAT)) == -1) {
        perror("ERROR on shmget");
        exit(EXIT_FAILURE);
    }

    // Attach to the SHM segment to get a pointer to it
    shm_data = shmat(shm_id, (void *) 0, 0);
    if (shm_data == (struct record *) -1) {
        perror("ERROR on shmat");
        exit(EXIT_FAILURE);
    }

    // Create child process for the IR communication
    if ((pid = fork()) == -1) {
        perror("ERROR on fork");
        exit(EXIT_FAILURE);
    }

    // This is for the child process only
    if (pid == 0) {
        unsigned long long time, last_time = 0, last_update = 0;
        int time_diff = 0;
        int stop_sent = 0;
        int keycode;

        while (1) {
            // Command is only the first character
            keycode = shm_data->msg[0];

            time = (unsigned long long) (shm_data->time.tv_sec * 1e6 + shm_data->time.tv_usec);
            time_diff = time - last_update;

            // Limit the number of messages but send STOP at any time only once
            if (time_diff > CMD_LOOP_WAIT || (keycode == KEYCODE_STOP && stop_sent == 0)) {
                // Use specific mode
                if (MODE == MODE_EXTENDED) {
                    proto_extended_mode(keycode);
                } else if (MODE == MODE_COMBO_DIRECT) {
                    proto_combo_direct_mode(keycode);
                } else if (MODE == MODE_SINGLE_OUTPUT) {
                    proto_single_output_mode(keycode);
                } else if (MODE == MODE_COMBO_PWM) {
                    proto_combo_pwm_mode(keycode);
                }

                // Make sure we send STOP only once
                if (keycode == KEYCODE_STOP) {
                    stop_sent = 1;
                } else {
                    stop_sent = 0;
                }

                last_update = time;
            } else if (time_diff == 0 || time == last_time) {
                // Waiting a bit to not to overload the CPU
                bcm2835_delayMicroseconds(CMD_LOOP_WAIT);
            }

            last_time = time;
        }

        _Exit(EXIT_SUCCESS);
    }

    while (1) {
        // Accept connections from clients
        if ((new_sock = accept(sock, (struct sockaddr *) &client, &client_len)) == -1) {
            perror("ERROR on accept");
            exit(EXIT_FAILURE);
        }

        // Get IP of the client
        inet_ntop(client.sin_family, get_in_addr((struct sockaddr *) &client), ip, sizeof ip);
        if (DEBUG > 0)
            printf("D: New connection from %s\n", ip);

        // Create child process for the network communication
        if ((pid = fork()) == -1) {
            perror("ERROR on fork");
            exit(EXIT_FAILURE);
        }

        // This is for the child process only
        if (pid == 0) {
            // Child doesn't need the server socket
            if (close(sock) == -1) {
                perror("ERROR on close");
                exit(EXIT_FAILURE);
            }

            // Read the message from the client socket
            read_client_msgs(new_sock, shm_data);

            _Exit(EXIT_SUCCESS);
        }

        // Close the new socket in the parent process
        if (close(new_sock) == -1) {
            perror("ERROR on close");
            exit(EXIT_FAILURE);
        }
    }

    // Clear the bus settings
    bcm2835_close();

    return EXIT_SUCCESS;
}

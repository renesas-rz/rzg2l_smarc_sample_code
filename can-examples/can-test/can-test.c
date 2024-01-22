#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/can/netlink.h>
#include <net/if.h>

#define CAN_INTERFACE_SEND "can0"
#define CAN_INTERFACE_RECV "can1"
#define NUM_MESSAGES        10000


static bool is_can_fd = false;
static int frame_len;
static int send_socket;
static int recv_socket;

static void print_usage(char *prg)
{
    fprintf(stderr,
		"%s RZ/G2L can0/can1 test program.\n"
		"    can0 will send data and can1 will receive/verify it.\n"
		"    classic CAN frame payload size is 8 bytes\n"
		"    CAN FD frame payload size is 64 bytes\n"
		"Usage: %s [options] \n"
		"\n"
		"Options:\n"
		"         -f       (use CAN FD frames instead of classic CAN)\n",
		prg,prg);

	exit(1);
}

// Function to setup the CAN interface
int setup_can_interface(void) {
    int ret;
    
    // down can0
    ret = system("ip link set can0 down");
    if (ret != 0) {
        fprintf(stderr, "Failed to bring up can0\n");
        return EXIT_FAILURE;
    }
    
    if (is_can_fd) 
        ret = system("ip link set can0 type can bitrate 1000000 dbitrate 3600000 fd on");
    else
        ret = system("ip link set can0 type can bitrate 1000000 dbitrate 1000000 fd on");
    if (ret != 0) {
        fprintf(stderr, "Failed to set bitrate for can0\n");
        return EXIT_FAILURE;
    }

    // up can0
    ret = system("ip link set can0 up");
    if (ret != 0) {
        fprintf(stderr, "Failed to bring up can0\n");
        return EXIT_FAILURE;
    }

    if (is_can_fd) 
        printf("CAN FD interface can0 has been set up with nominal bitrate 1Mbps and data bitrate 3.6Mbps, and is up.\n");
    else
        printf("CAN interface can0 has been set up with nominal bitrate 1Mbps and data bitrate 1Mbps, and is up.\n");

    // down can1
    ret = system("ip link set can1 down");
    if (ret != 0) {
        fprintf(stderr, "Failed to bring up can1\n");
        return EXIT_FAILURE;
    }
    
    if (is_can_fd) 
        ret = system("ip link set can1 type can bitrate 1000000 dbitrate 3600000 fd on");
    else
        ret = system("ip link set can1 type can bitrate 1000000 dbitrate 1000000 fd on");
    if (ret != 0) {
        fprintf(stderr, "Failed to set bitrate for can1\n");
        return EXIT_FAILURE;
    }

    // up can1
    ret = system("ip link set can1 up");
    if (ret != 0) {
        fprintf(stderr, "Failed to bring up can1\n");
        return EXIT_FAILURE;
    }

    if (is_can_fd) 
        printf("CAN FD interface can1 has been set up with nominal bitrate 1Mbps and data bitrate 3.6Mbps, and is up.\n");
    else
        printf("CAN interface can1 has been set up with nominal bitrate 1Mbps and data bitrate 1Mbps, and is up.\n");

    return 0;
}


// Function to get the current timestamp in milliseconds
long long current_timestamp() {
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec * 1000LL + te.tv_usec / 1000;
    return milliseconds;
}

// Thread function for sending CAN FD messages
void *can_send(void *arg) {
    struct ifreq ifr;
    struct sockaddr_can addr;
    struct can_frame frame_can;
    struct canfd_frame frame_fd;
    int i;
    long long start_time, end_time;
    int enable_socket_option = 1;
    	
    // Create a socket
    if ((send_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket");
        exit(1);
    }

    // Specify the CAN interface
    strcpy(ifr.ifr_name, CAN_INTERFACE_SEND);
    ioctl(send_socket, SIOCGIFINDEX, &ifr);

    if (is_can_fd) {
	if (setsockopt(send_socket, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
            &enable_socket_option, sizeof(enable_socket_option)) == -1) {
		 perror("setsockopt CAN_RAW_FD_FRAMES");
		 return NULL;
	}
	frame_fd.flags |= CANFD_BRS;
    }
	
    // Bind the socket to the CAN interface
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    
    if (bind(send_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Failed to bind socket to interface");
        close(recv_socket);
        return NULL;
    }

    start_time = current_timestamp();
    
    if (is_can_fd) {
        frame_fd.len = CANFD_MAX_DLEN;
        frame_fd.can_id = 0x123 | CAN_EFF_FLAG;
            
        // Send messages
        for (i = 0; i < NUM_MESSAGES; i++) {
            memset(frame_fd.data, i % 256, frame_len); // Fill the data with the incrementing value
            write(send_socket, &frame_fd, sizeof(struct canfd_frame)); // Send the CAN FD frame
            usleep(400);  
        }
    }
    else {
        frame_can.can_dlc = CAN_MAX_DLEN;
        frame_can.can_id = 0x123 | CAN_EFF_FLAG;
            
        // Send messages
        for (i = 0; i < NUM_MESSAGES; i++) {
            memset(frame_can.data, i % 256, frame_len); // Fill the data with the incrementing value
            write(send_socket, &frame_can, sizeof(struct can_frame)); // Send the CAN frame
            usleep(30);  
        }
    }

    end_time = current_timestamp();

    printf("can0 - Send %d frames, Duration: %lld ms, speed: %lldKB/s\n", \
        NUM_MESSAGES, end_time - start_time, (NUM_MESSAGES*frame_len)/(end_time - start_time));

    // Close the socket
    close(send_socket);
    return NULL;
}

// Thread function for receiving CAN FD messages
void *can_receive(void *arg) {
    struct ifreq ifr;
    struct sockaddr_can addr;
    struct can_frame frame_can;
    struct canfd_frame frame_fd;
    int nbytes, i;
    long long start_time, end_time;
    int enable_socket_option = 1;
	
    // Create a socket
    if ((recv_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket");
        exit(1);
    }

    // Specify the CAN interface
    strcpy(ifr.ifr_name, CAN_INTERFACE_RECV);
    ioctl(recv_socket, SIOCGIFINDEX, &ifr);

    if (is_can_fd) {
	if (setsockopt(recv_socket, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
            &enable_socket_option, sizeof(enable_socket_option)) == -1) {
		 perror("setsockopt CAN_RAW_FD_FRAMES");
		 return NULL;
	}
	//frame_fd.flags |= CANFD_BRS;
    }
    
    // Bind the socket to the CAN interface
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(recv_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Failed to bind socket to interface");
        close(recv_socket);
        return NULL;
    }
    
    // Receive messages
    start_time = current_timestamp();
    if (is_can_fd){
        for (i = 0; i < NUM_MESSAGES; i++) {
            nbytes = read(recv_socket, &frame_fd, sizeof(struct canfd_frame)); // Read the CAN FD frame
            if (nbytes > 0 && frame_fd.len == frame_len) {
                for (int j = 0; j < frame_len; j++) {
                    if (frame_fd.data[j] != (i % 256)) {
                        printf("CAN FD Data mismatch detected at message %d\n", i);
                        break;
                    }
                }
            }
            else
            {
                printf("Data mismatch detected at message %d\n", i);
                break;
            }
        }
    }
    else{
        for (i = 0; i < NUM_MESSAGES; i++) {
            nbytes = read(recv_socket, &frame_can, sizeof(struct can_frame)); // Read the CAN FD frame
            if (nbytes > 0 && frame_can.can_dlc == frame_len) {
                for (int j = 0; j < frame_len; j++) {
                    if (frame_can.data[j] != (i % 256)) {
                        printf("CAN Data mismatch detected at message %d\n", i);
                        break;
                    }
                }
            }
            else
            {
                printf("Data mismatch detected at message %d\n", i);
                break;
            }
        }
    }

    end_time = current_timestamp();

    printf("can1 - Receive %d frames, Duration: %lld ms, speed: %lldKB/s\n", \
        NUM_MESSAGES, end_time - start_time, (NUM_MESSAGES*frame_len)/(end_time - start_time));

    // Close the socket
    close(recv_socket);
    return NULL;
}

int main(int argc, char **argv) {
    pthread_t thread1, thread2;
    int opt;

    printf("%s [-f -h]\n", argv[0]);
    if (argc > 1) {    
	    while ((opt = getopt(argc, argv, "fh")) != -1){
		switch (opt){
		case 'f':
		    is_can_fd = true;
		    break;
		case 'h':
		default:
		    print_usage(argv[0]);
		    break;        
		}
	}
    }

    if (is_can_fd)
        frame_len = CANFD_MAX_DLEN;
    else
        frame_len = CAN_MAX_DLEN;
    printf("frame_len=%d\n",frame_len);
    
    // Setup CAN interface can0/can1 with a bitrate of 1Mbps, dbitrate of 2Mbps
    setup_can_interface();
    
    // Start the send and receive threads
    pthread_create(&thread1, NULL, can_send, NULL);
    pthread_create(&thread2, NULL, can_receive, NULL);

    // Wait for the threads to finish
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    return 0;
}


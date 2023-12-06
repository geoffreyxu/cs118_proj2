#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "utils.h"


int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct timeval tv;
    struct packet pkt;
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;
    unsigned short ack_num = 0;
    char last = 0;
    char ack = 0;

    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server
    
    // Initialize timeout values
    tv.tv_usec = 0;
    tv.tv_sec = TIMEOUT; 

    ssize_t bytes_read;
    ssize_t bytes_sent;
    ssize_t bytes_recv;

    int last_transmit_success = 1;

    while (1) {
        
        if(last_transmit_success){
            bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);
            last = feof(fp) ? 1 : 0;
            // Create a data packet
            build_packet(&pkt, seq_num, ack_num, last, ack, bytes_read, buffer);
        }

        bytes_sent = sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));

        if (bytes_sent < 0) {
            perror("Error sending data");
            break;
        }

        // Wait for acknowledgment
        setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        bytes_recv = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr*)&server_addr_from, &addr_size);

        if (bytes_recv < 0) {
            // Handle timeout or other errors
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout, handle retransmission or other actions
                printf("Timeout: Retransmitting...\n");
                last_transmit_success = 0;
                continue;
            } else {
                perror("Error receiving acknowledgment");
                break;
            }
        }

        // Process acknowledgment
        if (ack_pkt.ack == 1 && ack_pkt.acknum == seq_num) {
            printf("Acknowledgment received for sequence number %d\n", seq_num);
            seq_num++;
            ack_num++;
            last_transmit_success = 1;
        } else {
            printf("Incorrect acknowledgment received. Retransmit\n");
            last_transmit_success = 0;
        }

        if (last) {
            // All data sent, break from the loop
            printf("Success: All data sent\n");
            break;
        }
    }
 
    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}


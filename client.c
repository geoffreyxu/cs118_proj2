#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "utils.h"

/*long int find_num_packets(FILE* fp) {
    long int file_len;
    long int num_packets;
    fseek(fp, 0, SEEK_END);
    num_packets = ftell(fp) / PAYLOAD_SIZE + (file_len % PAYLOAD_SIZE);
}*/

/*int check_timeout(double start_time) {
    struct timeval ntv;
    gettimeofday(&ntv, NULL);
    return ((ntv.tv_sec + ntv.tv_usec / 1000000.0) - start_time >= TIMEOUT);
}*/

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

    int cwnd = 0; //will be incremented to 1 in first iteration
    int ssh = 6;
    int expected_ack_num = 1;
    int last_transmit_success = 1;
    ssize_t batch_bytes = 0;
    int seek_bytes = 0;

    int num_dup_acks = 0;
    int dup_ack = -1;
    //long int num_packets = find_num_packets(fp);
    //struct packet curr_pkt;
   // struct packet pkt_window[num_packets];
  //  curr_pkt = pkt_window[0];
    int start = 0;
    int end = 0;
    while (1) {

       /* if (check_timeout(pkt_window[start].start_time)) {
            ssh = MAX(cwnd / 2, 2);
            cwnd = 1;
            curr_pkt = pkt_window[start];
            struct timeval temp_tv;
            gettimeofday(&temp_tv, NULL);
            double start_time = temp_tv.tv_sec + temp_tv.tv_usec;
            curr_pkt.start_time = start_time;
            bytes_sent = sendto(send_sockfd, &curr_pkt, sizeof(curr_pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
            printf("Packet #%d lost, retransmitting\n", curr_pkt.seqnum);
            end = start;
        }*/

        if (!last_transmit_success){
            if (last){
                seek_bytes = -batch_bytes;
                seq_num -= batch_bytes/PAYLOAD_SIZE + 1;
                ack_num -= batch_bytes/PAYLOAD_SIZE + 1;
            }
            else{
                seek_bytes = (expected_ack_num - seq_num - 1) * PAYLOAD_SIZE;
                seq_num = expected_ack_num - 1;
                ack_num = expected_ack_num - 1;
            }
            fseek(fp, seek_bytes, SEEK_CUR);
            cwnd /= 2; //AIMD - Multiplicative Decrease
            last = 0;
        }
        else{
            cwnd++;
        }

        if (cwnd < 1) cwnd = 1;
        
        batch_bytes = 0;
        last_transmit_success = 1;

        //Create and send batch of packets
        for (int i = 0; i < cwnd && !last; i++) {
            bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);
            last = feof(fp) ? 1 : 0;
            build_packet(&pkt, seq_num, ack_num, last, ack, bytes_read, buffer);
            bytes_sent = sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
            if (bytes_sent < 0) {
                perror("Error sending data");
                break;
            }

            printf("Sending packet %d\n", seq_num);

            batch_bytes += bytes_read;
            seq_num++;
            ack_num++;

            sleep(0.1);
        }

        // Wait for acknowledgment
        setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        for (int i = 0; i < cwnd; i++){
            
            bytes_recv = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr*)&server_addr_from, &addr_size);
        
            if (bytes_recv < 0) {
                // Handle timeout or other errors
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Timeout, handle retransmission or other actions
                    printf("Expected Ack: %d; Timeout: Retransmitting...\n", expected_ack_num);
                    if (expected_ack_num <= seq_num)
                        last_transmit_success = 0;
                    break;
                } else {
                    perror("Error receiving acknowledgment");
                    break;
                }
            }
            // Process acknowledgment
            if (ack_pkt.ack == 1 && ack_pkt.acknum >= expected_ack_num) {
                printf("Acknowledgment received: %d\n", expected_ack_num);
                expected_ack_num = ack_pkt.acknum + 1;
            } else {
                printf("Expected: %d, Received: %d. Incorrect acknowledgment received. Retransmit\n", expected_ack_num, ack_pkt.acknum);
                //last_transmit_success = 0;
                //break;
            }
        }
        if (last && expected_ack_num == seq_num + 1) {
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


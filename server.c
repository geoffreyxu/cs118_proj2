#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

void send_ack(int sock_fd, struct packet* pkt, struct sockaddr_in addr, unsigned short ack_num, unsigned short seq_num, char last, char ack) {
    char payload[PAYLOAD_SIZE];
    memcpy(payload, (char*)&ack_num, sizeof(unsigned short));
    build_packet(pkt, seq_num, ack_num, last, ack, PAYLOAD_SIZE, payload);
    if (sendto(sock_fd, pkt, sizeof(*pkt), 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Error sending ACK");
        //close(sock_fd);
        return;
    }
    memset(payload, 0, PAYLOAD_SIZE);
    return;
}

int main() {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    struct packet buffer;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 0;
    //int recv_len;
    struct packet ack_pkt;
    int recv_window[FULL_WIND_SIZE];
    struct packet buffer_wind[FULL_WIND_SIZE];
    for (int i = 0; i < FULL_WIND_SIZE; i++) {
        recv_window[i] = 0;
    }
    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");

    // TODO: Receive file from the client and save it as output.txt
    //struct packet *pkt_cache;
    ssize_t bytes_recv;
    int error_cond = 0;
    int out_of_order_seqnum = -1;
    while (1) {
        if ((bytes_recv = recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr_from, &addr_size)) < 0) {
            perror("Error retrieving the packet\n");
            close(listen_sockfd);
            close(send_sockfd);
            error_cond = 1;
            break;
        }

        //Write received data to output
        printf("Expected Pkt #%d; Pkt #%d received\n", expected_seq_num, buffer.seqnum);
        if(buffer.seqnum == expected_seq_num) {
            int i = expected_seq_num;
            recv_window[expected_seq_num] = 1;
            buffer_wind[expected_seq_num] = buffer;
            for (; i < FULL_WIND_SIZE && recv_window[i] == 1; i++) {
              //  printf("%d\n", i);
                struct packet pkt = buffer_wind[i];
                fwrite(pkt.payload, 1, pkt.length, fp);
                if (pkt.last == 1) {
                    send_ack(send_sockfd, &ack_pkt, client_addr_to, pkt.acknum + 1, pkt.seqnum + 1, 1, 1);
                    fclose(fp);
                    close(listen_sockfd);
                    close(send_sockfd);
                    return 0;
                }
            }
            ack_pkt.acknum = i;
            expected_seq_num = i;
            printf("ACK #%d sent\n", buffer.acknum + 1);
            send_ack(send_sockfd, &ack_pkt, client_addr_to, buffer.acknum + 1, buffer.seqnum + 1, 0, 1);
        }
        else {
            if (buffer.seqnum > expected_seq_num) {
                out_of_order_seqnum = buffer.seqnum;
                recv_window[out_of_order_seqnum] = 1;
                buffer_wind[out_of_order_seqnum] = buffer;
            }
            ack_pkt.acknum = expected_seq_num;
            printf("ACK #%d. Wrong seq #%d\n", expected_seq_num, buffer.seqnum);
            send_ack(send_sockfd, &ack_pkt, client_addr_to, ack_pkt.acknum, ack_pkt.seqnum, 0, 1);
        }

    } 
    if (!error_cond) 
       // printf("Success: closing server");
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
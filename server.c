#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

void send_ack(int sock_fd, struct packet* pkt, struct sockaddr_in addr, unsigned short ack_num, unsigned short seq_num, char last, char ack) {
    char payload[PAYLOAD_SIZE];
    memcpy(payload, (char*)&ack_num, sizeof(unsigned int));
    build_packet(pkt, seq_num, ack_num, last, ack, PAYLOAD_SIZE, payload);
    if (sendto(sock_fd, pkt, sizeof(*pkt), 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Error sending ACK");
       // close(sock_fd);
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
    int recv_len;
    struct packet ack_pkt;

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

   // int seq_num = 0;
    //int ack_num = 0;

   // int n;
  /* if (connect(send_sockfd, (struct sockaddr *)&client_addr_to, sizeof(client_addr_to)) < 0) {
        perror("Client failed to connect to proxy server");
        fclose(fp);
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }*/
    int error_cond = 0;
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
        
        if (buffer.seqnum < expected_seq_num) {
            send_ack(send_sockfd, &ack_pkt, client_addr_to, expected_seq_num, expected_seq_num, 0, 1);
            continue;
        }
        else if(buffer.seqnum == expected_seq_num){
            fwrite(buffer.payload, 1, buffer.length, fp);
            send_ack(send_sockfd, &ack_pkt, client_addr_to, buffer.acknum + 1, buffer.seqnum + 1, 0, 1);
            expected_seq_num = buffer.seqnum + 1;
            if (buffer.last == 1) {
                break;
            }
        }
        //ack_num = buffer.acknum + 1;
        /*fwrite(buffer.payload, 1, buffer.length, fp);
        printf("Pkt #%d received\n", buffer.seqnum);
        if (expected_seq_num == buffer.seqnum) {
            memcpy(&(pkt_cache[buffer.seqnum - expected_seq_num]), &buffer, sizeof(struct packet));
            if (buffer.last == 1) {
                build_packet(&ack_pkt, 0, expected_seq_num, 0, 1, "", -1);
                break;
            }   
        }*/

    } 
    if (!error_cond) 
        printf("Success: closing server");
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
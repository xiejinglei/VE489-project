// TODO: implement part 3.1

#include <stdint.h>
#include "./crc32.h"
#include "./SRHeader.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>
#include <time.h>


const int HEADER_SIZE = sizeof(struct SRHeader);
const int MAX_PACKET_SIZE = sizeof(struct SRHeader) + MAX_PAYLOAD_SIZE;
const char* FLAGS[] = {"SYN", "FIN", "DATA", "ACK", "NACK"};


// find the start of new sliding window (sender)
int get_new_window_start(int *arr, int arr_size, int old_window_start)
{
    int ret = old_window_start;
    while (ret < arr_size && arr[ret] == 1)
    {
        ret++;
    }
    return ret;
}

// find the start of new sliding window (receiver)
int get_new_window_start_recv(char **window_buffer, int window_size, int old_window_start)
{
    int index = 0;
    while (index < window_size && window_buffer[index] != NULL)
    {
        index++;
    }
    return (old_window_start + index);
}

// check if all data are acked
int is_all_acked(int *arr, int arr_size)
{
    int cnt = 0;
    for (int i = 0; i < arr_size; i++)
    {
        if (arr[i] == 1)
            cnt++;
    }
    return (cnt == arr_size);
}


// run receiver
int run_receiver(const int port, const int window_size, const char* recv_dir, const char* log_file)
{
    // configuration
    struct sockaddr_in si_me;
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == -1) {
        fputs("socket creation failed!", stderr);
        exit(2);
    }
    memset((char *) &si_me, 0, sizeof(struct sockaddr_in));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(sockfd , (struct sockaddr*) &si_me, sizeof(struct sockaddr_in));
    
    socklen_t slen = sizeof(struct sockaddr_in);
    struct sockaddr_in si_other;

    /* ================================ Begin Receving ================================ */

    char *buf = malloc(MAX_PACKET_SIZE);    // a buffer for a packet

    FILE* fp;           // file to be written
    FILE* fp_log;       // log file
    int cnt_file = 0;   // count number of files
    int fp_open = 0;    // 0: file not open     1: file open

    fp_log = fopen(log_file, "w");

    // data parameters
    int window_start = 0;   // start of current window
    char **window_buffer_payload = malloc(sizeof(char*)*window_size);  // store packets of this window
    for (int i = 0; i < window_size; i++)
        window_buffer_payload[i] = NULL;    // initialize as NULL: no data
    struct SRHeader* window_buffer_header = malloc(HEADER_SIZE*window_size);   // store headers of this window

    while (1) 
    {
        int n = recvfrom(sockfd, buf, MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sockaddr *) &si_other, &slen);

        if (n > 0) {
            // parse header
            struct SRHeader header;
            memcpy(&header, buf, HEADER_SIZE);
            //printf("%d, %d, %d, %d\n", header.flag, header.seq, header.len, header.crc);
            fprintf(fp_log, "%s %d %d %d\n", FLAGS[header.flag], header.seq, header.len, header.crc);
            fflush(fp_log);

            // get SYN: send ACK
            if (header.flag == SYN)
            {
                int ack_syn = crc32(NULL, 0);
                struct SRHeader ack_header = {.flag = ACK, .seq = header.seq, .len = 0, .crc = ack_syn};
                sendto(sockfd, (void*) &ack_header, HEADER_SIZE, 0, (const struct sockaddr *) &si_other, 
                        sizeof(struct sockaddr_in));
                fprintf(fp_log, "%s %d %d %d\n", FLAGS[ack_header.flag], ack_header.seq, ack_header.len, 
                    ack_header.crc);
                fflush(fp_log);
                
                if (fp_open == 0)
                {
                    // file is not opened: open file for writing
                    cnt_file++;

                    char filename[100];
                    strcpy(filename, recv_dir);
                    strcat(filename, "file_");
                    char tmpstr[20];
                    sprintf(tmpstr, "%d", cnt_file);
                    strcat(filename, tmpstr);
                    strcat(filename, ".txt");
                    fp = fopen(filename, "wb");

                    fp_open = 1;

                    if (!fp)
                    {
                        printf("File %s cannot be opened.\n", filename);
                        exit(-1);
                    }
                }
            }

            // get data packets
            if (header.flag == DATA)
            {
                if (header.seq >= window_start + window_size)
                    continue;   // drop packet

                char *data_buf = malloc(MAX_PAYLOAD_SIZE);  // store payload
                memcpy(data_buf, buf+HEADER_SIZE, MAX_PAYLOAD_SIZE);

                // check crc. Not match: drop
                int crc_check = crc32(data_buf, header.len);
                if (crc_check != header.crc)
                {
                    //printf("DEBUG: crc does not match!");
                    free(data_buf);
                    continue;
                }

                if (header.seq < window_start)
                {
                    // ack window_start; does not buffer
                    int ack_syn = crc32(NULL, 0);
                    struct SRHeader ack_header = {.flag = ACK, .seq = window_start, .len = 0, .crc = ack_syn};
                    sendto(sockfd, (void*) &ack_header, HEADER_SIZE, 0, (const struct sockaddr *) &si_other, 
                        sizeof(struct sockaddr_in));
                    fprintf(fp_log, "%s %d %d %d\n", FLAGS[ack_header.flag], ack_header.seq, ack_header.len, 
                        ack_header.crc);
                    fflush(fp_log);

                    free(data_buf);
                    continue;
                }  

                if (header.seq > window_start && header.seq < window_start + window_size)
                {
                    // ack window_start; buffer it in window.
                    int ack_syn = crc32(NULL, 0);
                    struct SRHeader ack_header = {.flag = ACK, .seq = window_start, .len = 0, .crc = ack_syn};
                    sendto(sockfd, (void*) &ack_header, HEADER_SIZE, 0, (const struct sockaddr *) &si_other, 
                        sizeof(struct sockaddr_in));
                    fprintf(fp_log, "%s %d %d %d\n", FLAGS[ack_header.flag], ack_header.seq, ack_header.len, 
                        ack_header.crc);
			fflush(fp_log);

                    // store header
                    window_buffer_header[header.seq-window_start] = header;

                    // store data
                    window_buffer_payload[header.seq-window_start] = data_buf;
                } 

                if (header.seq == window_start)
                {
                    // store header
                    window_buffer_header[0] = header;

                    // store data
                    window_buffer_payload[0] = data_buf;

                    // get new window start
                    int new_window_start = get_new_window_start_recv(window_buffer_payload, window_size,
                        window_start);

                    // ack new_window_start
                    int ack_syn = crc32(NULL, 0);
                    struct SRHeader ack_header = {.flag = ACK, .seq = new_window_start, .len = 0, .crc = ack_syn};
                    sendto(sockfd, (void*) &ack_header, HEADER_SIZE, 0, (const struct sockaddr *) &si_other, 
                        sizeof(struct sockaddr_in));
                    fprintf(fp_log, "%s %d %d %d\n", FLAGS[ack_header.flag], ack_header.seq, ack_header.len, 
                        ack_header.crc);
                    fflush(fp_log);

                    // write buffer to file
                    int num_write = new_window_start - window_start;
                    for (int k = 0; k < num_write; k++)
                    {
                        fwrite(window_buffer_payload[k], 1, window_buffer_header[k].len, fp);
                        fflush(fp);

                        // clean buffer
                        free(window_buffer_payload[k]);
                        window_buffer_payload[k] = NULL;
                    }

                    // rearrange window
                    for (int k = num_write; k < window_size; k++)
                    {
                        window_buffer_payload[k-num_write] = window_buffer_payload[k];
                        window_buffer_header[k-num_write] = window_buffer_header[k]; 

                        //window_buffer_payload[k] = NULL;    // clean
                    }
                    for (int k = window_size-1; k >= window_size-num_write; k--)
                    {
                        // clean
                        window_buffer_payload[k] = NULL;
                    }

                    // advance window
                    window_start = new_window_start;

                }
            }
            
            // get FIN: send ACK
            if (header.flag == FIN)
            {
                int ack_syn = crc32(NULL, 0);
                struct SRHeader ack_header = {.flag = ACK, .seq = header.seq, .len = 0, .crc = ack_syn};
                sendto(sockfd, (void*) &ack_header, HEADER_SIZE, 0, (const struct sockaddr *) &si_other, 
                        sizeof(struct sockaddr_in));
                fprintf(fp_log, "%s %d %d %d\n", FLAGS[ack_header.flag], ack_header.seq, ack_header.len, 
                    ack_header.crc);
                fflush(fp_log);
                
                if (fp_open == 1)
                {
                    // file open: close it
                    fp_open = 0;
                    fclose(fp);
                    // TODO: clean data buffer and reset everything
                    window_start = 0;     
                    for (int i = 0; i < window_size; i++)
                        window_buffer_payload[i] = NULL;    
                }
            }
        }
    }

    fclose(fp_log);
    //free(window_buffer);
    free(buf);
    return 0;
}

int run_sender(const char* receiver_ip, const int receiver_port, const int sender_port, 
                int window_size, const char* file_name, const char* log_file)
{
    // configuration
    // create UDP socket.
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // use IPPROTO_UDP
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(1);
    }

    // below is the same as TCP socket.
    struct sockaddr_in myaddr;
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    // bind to a specific port
    myaddr.sin_port = htons(sender_port);
    bind(sockfd, (struct sockaddr *) & myaddr, sizeof(myaddr));

    struct sockaddr_in receiver_addr;
    struct hostent *host = gethostbyname(receiver_ip);
    memcpy(&(receiver_addr.sin_addr), host->h_addr, host->h_length);
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(receiver_port);


    /* ================================ Begin Sending ================================*/

    // log file
    FILE *fp_log;
    fp_log= fopen(log_file, "w");
 
    if(fp_log == NULL){
        printf("Error opening log file of sender.\n");
        return -1;
    }

    char *buf = malloc(MAX_PACKET_SIZE);    // buffer for reading ack

    // send SYN packet with a random seq value
    srand(time(NULL));
    int seq_syn = rand();
    int crc_syn = crc32(NULL, 0);
    struct SRHeader syn_header = {.flag = SYN, .seq = seq_syn, .len = 0, .crc = crc_syn};
    sendto(sockfd, (void*) &syn_header, HEADER_SIZE, 0, (const struct sockaddr *) &receiver_addr, 
            sizeof(struct sockaddr_in));
    fprintf(fp_log, "%s %d %d %d\n", FLAGS[syn_header.flag], syn_header.seq, syn_header.len, syn_header.crc);
    fflush(fp_log);

    // wait for ack
    struct timeval start, stop;
    gettimeofday(&start, NULL);
    gettimeofday(&stop, NULL);
    
    while (1)
    {
        socklen_t slen = sizeof(struct sockaddr_in);
        int n = recvfrom(sockfd, buf, MAX_PACKET_SIZE, MSG_DONTWAIT, 
                            (struct sockaddr *) &receiver_addr, &slen);
        if (n > 0)
        {
            struct SRHeader header;
            memcpy(&header, buf, HEADER_SIZE);
            //printf("%d, %d, %d, %d\n", header.flag, header.seq, header.len, header.crc);
            fprintf(fp_log, "%s %d %d %d\n", FLAGS[header.flag], header.seq, header.len, header.crc);
            fflush(fp_log);
            break;
        }

        gettimeofday(&stop, NULL);
        // time out, resend
        if ((double)(stop.tv_sec-start.tv_sec)*1000 + (double)(stop.tv_usec-start.tv_usec) / 1000 > 400)
        {
            sendto(sockfd, (void*) &syn_header, HEADER_SIZE, 0, (const struct sockaddr *) &receiver_addr, 
                    sizeof(struct sockaddr_in));
            fprintf(fp_log, "%s %d %d %d\n", FLAGS[syn_header.flag], syn_header.seq, syn_header.len, 
                    syn_header.crc);
            fflush(fp_log);
            gettimeofday(&start, NULL);     // reset start time
            gettimeofday(&stop, NULL);
        }
    }


    // read file
    FILE *fp;
    fp = fopen(file_name, "rb");
 
    if(fp == NULL){
        printf("Error opening file.\n");
        return -1;
    }

    // Get file length and calculate number of packets needed
    fseek(fp, 0L, SEEK_END);
    long length = ftell(fp);
    rewind(fp);
    int num_packets = length / MAX_PAYLOAD_SIZE;
    int last_full = 0;  // If the last packet is full (MAX_PACKET_SIZE), then it is 1.
    if (length != num_packets*MAX_PAYLOAD_SIZE)
        num_packets++;
    else
        last_full = 1;

    // DEBUG
    printf("num_packets: %d\n", num_packets);

    // file is too small
    if (num_packets < window_size)
        window_size = num_packets;
    

    // get headers and data
    char **buf_arr = malloc(sizeof(char*)*num_packets);  // store all packet buffers
    struct SRHeader *data_headers = malloc(HEADER_SIZE*num_packets);

    for (int i = 0; i < num_packets; i++)
    {
        char* data_buffer = malloc(MAX_PACKET_SIZE);
        struct SRHeader data_header = {.flag = DATA, .seq = i, .len = MAX_PAYLOAD_SIZE, .crc = 0};
        if (last_full == 0 && i == num_packets-1)
            data_header.len = length - (num_packets-1)*MAX_PAYLOAD_SIZE;

        fread(buf, 1, MAX_PAYLOAD_SIZE, fp);
        data_header.crc = crc32(buf, (size_t)data_header.len);
        data_headers[i] = data_header;

        memcpy(data_buffer + HEADER_SIZE, buf, MAX_PAYLOAD_SIZE);
        memcpy(data_buffer, &data_header, HEADER_SIZE);

        buf_arr[i] = data_buffer;
    }


    // send data packets
    int *data_ack = (int*)malloc(sizeof(int)*num_packets);    // 1 if packet is acked
    int window_start = 0;   // The start postition of window
    //int cnt_acked_packets = 0;
    for (int i = 0; i < window_size; i++)   
    {
        // send all packets of the first window
        sendto(sockfd, (void*)buf_arr[i], MAX_PACKET_SIZE, 0, (const struct sockaddr *) &receiver_addr, 
            sizeof(struct sockaddr_in));

        struct SRHeader header = data_headers[i];
        fprintf(fp_log, "%s %d %d %d\n", FLAGS[header.flag], header.seq, header.len, header.crc);
        fflush(fp_log);
    }

    gettimeofday(&start, NULL);     // reset start time
    gettimeofday(&stop, NULL);

    while (is_all_acked(data_ack, num_packets) == 0)
    {
        socklen_t slen = sizeof(struct sockaddr_in);
        int n = recvfrom(sockfd, buf, MAX_PACKET_SIZE, MSG_DONTWAIT, 
                            (struct sockaddr *) &receiver_addr, &slen);

        if (n > 0)
        {
            // receive ack
            struct SRHeader header;
            memcpy(&header, buf, HEADER_SIZE);
            //printf("%d, %d, %d, %d\n", header.flag, header.seq, header.len, header.crc);
            fprintf(fp_log, "%s %d %d %d\n", FLAGS[header.flag], header.seq, header.len, header.crc);
            fflush(fp_log);

            // if ACK's seq = 0, then packet 0 is not received
            if (header.seq != 0)    
            {
                for (int j = window_start; j < header.seq; j++)
                    data_ack[j] = 1;     // ACK x: all packets before x are received.

                // check if finish
                if (is_all_acked(data_ack, num_packets) == 1)
                    break;

                // check if the window can be slided
                int new_window_start = get_new_window_start(data_ack, num_packets, window_start);
                if (new_window_start > window_start)
                {
                    int num_new_send = new_window_start - window_start;
                    int index_send_start = window_start + window_size;
                    //int index_send_start = window_start + 
                    int index_send_end = index_send_start + num_new_send;

                    // check if it is the last window
                    if (index_send_start >= num_packets)
                        continue;
                    if (index_send_end > num_packets)
                        index_send_end = num_packets;

                    window_start = new_window_start;
                    //DEBUG
                    //printf("window_start: %d\n", window_start);

                    // send data of new window
                    for (int j = index_send_start; j < index_send_end; j++)
                    {
                        sendto(sockfd, (void*)buf_arr[j], MAX_PACKET_SIZE, 0, 
                            (const struct sockaddr *) &receiver_addr, sizeof(struct sockaddr_in));
                        struct SRHeader header_tmp = data_headers[j];

                        /*
                        //DEBUG
                        if (header_tmp.flag == SYN)
                        {
                            printf("SYN!!!!!!\n");
                            printf("seq: %d, len: %d\n", header_tmp.seq, header_tmp.len);
                            printf("index_send_end: %d, num_packets: %d\n", index_send_end, num_packets);
                            exit(-1);
                        }*/


                        fprintf(fp_log, "%s %d %d %d\n", FLAGS[header_tmp.flag], header_tmp.seq, 
                            header_tmp.len, header_tmp.crc);
                        fflush(fp_log);
                    }

                    gettimeofday(&start, NULL);     // reset start time
                    gettimeofday(&stop, NULL);
                }
            }
        }

        // timeout: resend whole window
        gettimeofday(&stop, NULL);
        if ((double)(stop.tv_sec-start.tv_sec)*1000 + (double)(stop.tv_usec-start.tv_usec) / 1000 > 400)
        {
            int send_end = window_start+window_size;
            if (send_end > num_packets)
                send_end = num_packets;
            for (int j = window_start; j < send_end; j++)   
            {
                // resend all packets of the window
                sendto(sockfd, (void*)buf_arr[j], MAX_PACKET_SIZE, 0, (const struct sockaddr *) &receiver_addr, 
                    sizeof(struct sockaddr_in));

                struct SRHeader header_tmp = data_headers[j];
                fprintf(fp_log, "%s %d %d %d\n", FLAGS[header_tmp.flag], header_tmp.seq, 
                    header_tmp.len, header_tmp.crc);
                fflush(fp_log);
            }
            gettimeofday(&start, NULL);     // reset start time
            gettimeofday(&stop, NULL);
        }
    }

    fclose(fp);


    // send FIN packet
    struct SRHeader fin_header = {.flag = FIN, .seq = seq_syn, .len = 0, .crc = crc_syn};
    sendto(sockfd, (void*) &fin_header, HEADER_SIZE, 0, (const struct sockaddr *) &receiver_addr, 
            sizeof(struct sockaddr_in));
    fprintf(fp_log, "%s %d %d %d\n", FLAGS[fin_header.flag], fin_header.seq, fin_header.len, fin_header.crc);
    fflush(fp_log);

    // wait for ack
    gettimeofday(&start, NULL);     // reset start time
    gettimeofday(&stop, NULL);
    while (1)
    {
        socklen_t slen = sizeof(struct sockaddr_in);
        int n = recvfrom(sockfd, buf, MAX_PACKET_SIZE, MSG_DONTWAIT, 
                            (struct sockaddr *) &receiver_addr, &slen);
        if (n > 0)
        {
            struct SRHeader header;
            memcpy(&header, buf, HEADER_SIZE);
            fprintf(fp_log, "%s %d %d %d\n", FLAGS[header.flag], header.seq, header.len, header.crc);
            fflush(fp_log);
            //printf("%d, %d, %d, %d\n", header.flag, header.seq, header.len, header.crc);
            if (header.seq == fin_header.seq)
                break;      // confirm it's ack of fin
        }

        gettimeofday(&stop, NULL);
        // time out, resend
        if ((double)(stop.tv_sec-start.tv_sec)*1000 + (double)(stop.tv_usec-start.tv_usec) / 1000 > 400)
        {
            sendto(sockfd, (void*) &fin_header, HEADER_SIZE, 0, (const struct sockaddr *) &receiver_addr, 
                    sizeof(struct sockaddr_in));
            fprintf(fp_log, "%s %d %d %d\n", FLAGS[fin_header.flag], fin_header.seq, fin_header.len, 
                fin_header.crc);
            fflush(fp_log);

            gettimeofday(&start, NULL);     // reset start time
            gettimeofday(&stop, NULL);
        }
    }

    free(buf);
    for (int i = 0; i < num_packets; i++)
    {
        free(buf_arr[i]);
    }
    free(buf_arr);
    free(data_headers);
    fclose(fp_log);

    return 0;
}



int main(int argc, char **argv)
{
    // Parse command line arguments
	if (argc != 6 && argc != 8) {
		printf("As receiver: ./sr -r <port> <window size> <recv dir> <log file>\n");
        printf("As sender: ./sr -s <receiver’s IP> <receiver’s port> <sender’s port> <window size> <file to send> <log file>\n");
		return 1;
	}

    // receiver mode
    if (argc == 6)
    {
        const int port = atoi(argv[2]);
        const int window_size = atoi(argv[3]);
        const char* recv_dir = argv[4];
        const char* log_file = argv[5];

        int ret = run_receiver(port, window_size, recv_dir, log_file);   
        return ret;     
    }

    // sender mode
    if (argc == 8)
    {
        const char* receiver_ip = argv[2];
        const int receiver_port = atoi(argv[3]);
        const int sender_port = atoi(argv[4]);
        const int window_size = atoi(argv[5]);
        const char* file_name = argv[6];
        const char* log_file = argv[7];

        int ret = run_sender(receiver_ip, receiver_port, sender_port, window_size, file_name, log_file);
        return ret;
    }

    return 0;
}

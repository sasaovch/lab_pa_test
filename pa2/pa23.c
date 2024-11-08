#include "banking.h"
#include "common.h"
#include "pa2345.h"

#include "pipes_const.h"
#include "time_work.h"
#include "ipc.h"
#include "child_work.h"
#include "parent_work.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>

#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

void transfer(void * parent_data, local_id src, local_id dst,
              balance_t amount)
{
    // Info *info = (Info *) parent_data;
    (void) parent_data;
    pipe_info.local_time++;

    TransferOrder order;
    order.s_src = src;
    order.s_dst = dst;
    order.s_amount = amount;
    
    Message msg;
    msg.s_header.s_magic = MESSAGE_MAGIC;
    msg.s_header.s_payload_len = sizeof(TransferOrder);
    msg.s_header.s_type = TRANSFER;
    msg.s_header.s_local_time = get_lamport_time();

    printf("--- %d: %d transfer from %d to %d amount %d\n", get_lamport_time(), pipe_info.fork_id, src, dst, amount);

    memcpy(&msg.s_payload, &order,sizeof(TransferOrder));

    if (pipe_info.fork_id == 0) {
        Message received_message;
        int16_t type = -1;

        // send(&pipe_info, src, &msg);

        size_t message_size_hs = sizeof(MessageHeader) + msg.s_header.s_payload_len;

        write(pipe_info.pm[pipe_info.fork_id][src][1], &msg, message_size_hs);                
        
        pipe_info.local_time++;
        type = receive_any(&pipe_info, &received_message);
            
        // fprintf(elf, "Parent got message with type: %d\n", type);
        // fflush(elf);
        // fprintf(elf, "Parent got message with payload: %s\n", received_message.s_payload);
        // fflush(elf);

        if (received_message.s_header.s_type == ACK) {
            // printf("--- %d: %d parent received from %d to %d at time %d\n", get_lamport_time(), pipe_info.fork_id, src, dst, received_message.s_header.s_local_time);
            sync_lamport_time(&pipe_info, received_message.s_header.s_local_time);
            // printf("--- %d: %d updated time\n", get_lamport_time(), pipe_info.fork_id);

            // fprintf(elf, "Parent received ACK message\n");
            // fflush(elf);
            type = ACK;
            return;
        }
    }
    if (pipe_info.fork_id == src) {
        // send(&pipe_info, dst, &msg);

        size_t message_size_hs = sizeof(MessageHeader) + msg.s_header.s_payload_len;

        write(pipe_info.pm[pipe_info.fork_id][dst][1], &msg, message_size_hs);                
    }
}

int is_not_child(int fork_id) {
    return fork_id == 0;
}

int main(int argc, char * argv[])
{
    int N = atoi(argv[2]) + 1;
    
    int *array = (int *) malloc((N) * sizeof(int));
    array[0] = N;
    for (int i = 1; i < N; i++) array[i] = atoi(argv[i + 2]);

    elf = fopen(events_log, "a");
    plf = fopen(pipes_log, "a");

    srand(time(NULL));
    int r = rand();        

    fprintf(elf, "-------------------- VERSION 1.3.1 tag:%d --------------\n", r);
    fflush(elf);

    fprintf(stdout, "-------------------- VERSION 1.3.2 tag:%d --------------\n", r);
    fflush(stdout);

    local_id line = 0;
    local_id column = 0;
    int all_pipes_number = N;

    while (line < all_pipes_number) {
        column = 0;
        while (column < all_pipes_number) {
            if (line == column) {
                pm[line][column][0] = -1;
                pm[line][column][1] = -1;
                column++;
            } else {
                int descriptors[2];
                pipe(descriptors);

                fcntl(descriptors[0], F_SETFL, fcntl(descriptors[0], F_GETFL, 0) | O_NONBLOCK);
                fcntl(descriptors[1], F_SETFL, fcntl(descriptors[1], F_GETFL, 0) | O_NONBLOCK);

                for (int i = 0; i < 2; i++) {
                    pm[line][column][i] = descriptors[i]; // 0 - read and 1 - write
                }

                fprintf(plf, "Pipe %d -> %d. Fd %d -> %d\n", line, column, descriptors[0], descriptors[1]);
                fflush(plf);
                column++;
            }
        }
        line++;
    }

    fprintf(elf, "-------------------- write --------------\n");
    fflush(elf);

    fprintf(stdout, "-------------------- write --------------\n");
    fflush(stdout);  

    // Message start_msg;
    // char start_message[MAX_PAYLOAD_LEN];

    // sprintf(start_message, log_started_fmt, 0, 0, 0, 0, 0);
    // memset(start_msg.s_payload, '\0', sizeof(char)*(MAX_PAYLOAD_LEN));
    // memcpy(start_msg.s_payload, start_message, sizeof(char)*(MAX_PAYLOAD_LEN));    

    // start_msg.s_header.s_magic = MESSAGE_MAGIC;
    // start_msg.s_header.s_payload_len = MAX_PAYLOAD_LEN + 1;
    // start_msg.s_header.s_type = STARTED; 
    // size_t message_size = sizeof(MessageHeader) + start_msg.s_header.s_payload_len; 

    // write(pm[0][1][1], &start_msg, message_size);    

    fprintf(elf, "-------------------- wrote --------------\n");
    fflush(elf);

    fprintf(stdout, "-------------------- wrote --------------\n");
    fflush(stdout);      

    local_id number_id = 1;
    while (number_id < N) {
        int fork_id = fork();
        if (!is_not_child(fork_id)) {
            number_id++;
        } else {
            ChildState child_state = {
                .fork_id = number_id,
                .child_time = 0,
                .N = N,
                .balance_history = {
                    .s_id = number_id,
                    .s_history_len = 1,
                    .s_history[0] = {
                        .s_balance = array[number_id],
                        .s_time = 0,
                        .s_balance_pending_in = 0,
                    }
                }
            };

            fprintf(elf, "-------------------- INIT CHILD 1 %d --------------\n", number_id);
            fflush(elf);

            fprintf(stdout, "-------------------- INIT CHILD 2 %d --------------\n", number_id);
            fflush(stdout);            

            init_child_work(&child_state);

            fprintf(elf, "-------------------- HANDLE CHILD 1 %d --------------\n", number_id);
            fflush(elf);

            fprintf(stdout, "-------------------- HANDLE CHILD 2 %d --------------\n", number_id);
            fflush(stdout);            

            handle_transfers(&child_state);

            fprintf(elf, "-------------------- FINISH CHILD 1 %d --------------\n", number_id);
            fflush(elf);

            fprintf(stdout, "-------------------- FINISH CHILD 2 %d --------------\n", number_id);
            fflush(stdout);   
            return 0;
        }
    }

    // Start parent
    pipe_info.fork_id = 0;
    pipe_info.N = N;
    pipe_info.local_time = 0;
    for(int i = 0; i < 10; i++) {
        for(int j = 0; j < 10; j++) {
            for(int k = 0; k < 2; k++) {
                pipe_info.pm[i][j][k] = pm[i][j][k];
            }
        }
    }

    fprintf(elf, "-------------------- INIT PARENT 1 %d --------------\n", number_id);
    fflush(elf);

    fprintf(stdout, "-------------------- INIT PARENT 2 %d --------------\n", number_id);
    fflush(stdout);   

    init_parent_work(&pipe_info, N);

    fprintf(elf, "-------------------- WORK PARENT 1 %d --------------\n", number_id);
    fflush(elf);

    fprintf(stdout, "-------------------- WORK PARENT 2 %d --------------\n", number_id);
    fflush(stdout);   

    do_parent_work(&pipe_info, N);

    fprintf(elf, "-------------------- HISTORY PARENT 1 %d --------------\n", number_id);
    fflush(elf);

    fprintf(stdout, "-------------------- HISTORY PARENT 2 %d --------------\n", number_id);
    fflush(stdout);   

    print_history_from_all_children(&pipe_info, N);

    fprintf(elf, "-------------------- WAIT PARENT 1 %d --------------\n", number_id);
    fflush(elf);

    fprintf(stdout, "-------------------- WAIT PARENT 2 %d --------------\n", number_id);
    fflush(stdout);   

    parent_are_waiting(&pipe_info, N);
    
    free(array);

    fprintf(elf, "-------------------- FINISH PARENT 1 %d --------------\n", number_id);
    fflush(elf);

    fprintf(stdout, "-------------------- FINISH PARENT 2 %d --------------\n", number_id);
    fflush(stdout);   

    return 0;
}

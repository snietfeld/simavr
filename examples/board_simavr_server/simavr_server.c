/* simavr_server.cpp */

#include "simavr_server.h"

#include <stdio.h>
#include <time.h>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "sim_avr.h"
#include "cpu.h"

extern avr_t* avr;

enum MsgType {
  NONE=0,
  INIT=1,
  READ_DATA=2,
  WRITE_DATA=3,
  STEP=4,
  KILL=5,
  INIT_OK=6,
  INIT_FAILED=7,
  READ_DATA_OK=8,
  WRITE_DATA_OK=9,
  STEP_OK=10,
  KILL_OK=11
};

struct msg_buffer {
  long msg_type;
  char msg_text[SRAM_BYTES];
} message;

void step(uint64_t N_steps){
  static uint64_t instruction_count = 0;

  if (avr->state != cpu_Running)
    return;
  
  for (uint64_t i = 0; i < N_steps; i++){
    int state = avr_run(avr);
    
    //printf("CPU State: %d\n", state);
    if (state == cpu_Done) {
      printf("CPU DONE\n");
      printf("Instructions: %ld\n", instruction_count);
      break;
    }
    else if (state == cpu_Crashed) {
      printf("CPU CRASHED\n");
      break;
    }
    instruction_count++;
  }
  //printf("CPU emulator yielding at instruction count: %ld\n", instruction_count);  
}

int main(int argc, char* argv[])
{
  for (int i=0; i < argc; i++){
    printf("Arg %d: %s\n", i, argv[i]);
  }

  if (argc != 5){
    printf("Error: Expected 4 arguments, received %d\n", argc-1);
    return -1;
  }

  const char* firmware_path = argv[1];  // Path to firmware hex
  key_t in_key = atoi(argv[2]);         // Input message queue key
  key_t out_key = atoi(argv[3]);        // Output message queue key
  const char* log_path = argv[4];        // Path to redirect output to

  int log_fd = open(log_path, O_WRONLY);
  if (log_fd){
    dup2(log_fd, fileno(stdout));  // redirect stdout
    dup2(log_fd, fileno(stderr));
  }
  else{
    printf("SERVER: Error - Unable to open %s for writing.\n", log_path);
  }

  
  // Create message queue
  printf("SERVER: Creating message queues...\n");

  int in_msgid = msgget(in_key, 0666);
  if (in_msgid == -1) {
    printf("SERVER: Error creating CLIENT2SERVER message queue: %d:(%s)\n",
	    errno, strerror(errno));
    return -1;
  }

  int out_msgid = msgget(out_key, 0666);
  if (in_msgid == -1) {
    printf("SERVER: Error creating SERVER2CLIENT message queue: %d:(%s)\n",
	    errno, strerror(errno));
    return -1;
  }

  printf("SERVER: Message queues created successfully.\n");


  // Initialize AVR emulator
  printf("SERVER: Initializing AVR with firmware: %s\n", firmware_path);
  avr = init_cpu(firmware_path);

  if (avr != NULL) {
    printf("SERVER: AVR Initialized.\n");
  }
  else {
    printf("SERVER: AVR initialization FAILED. Exiting...\n");
    return -1;
  }

  
  
  // MAIN LOOP
  int kill_requested = 0;
  
  while(!kill_requested){
    // Check for new message
    //printf("SERVER: Waiting for message...\n");
    if (msgrcv(in_msgid, &message, sizeof(message), 0, 0) == -1){
      printf("SERVER: Error: msgrcv() returned -1\nExiting...\n");
      break;
    }
  
    //printf("SERVER: Received message type %ld: %s \n", message.msg_type, message.msg_text);
  
    // Process message
    enum MsgType t = message.msg_type;
    switch(t){
    case NONE:
      printf("SERVER: Error - message type is NONE\n");
      break;
    case INIT:
      {
	//printf("SERVER: Received INIT message...\n");

	if (avr != NULL) {
	  //printf("SERVER: Sending INIT_OK...\n");
	  message.msg_type = INIT_OK;
	}
	else {
	  //printf("SERVER: Sending INIT_FAILED...\n");
	  message.msg_type = INIT_FAILED;
	}
	
	if (msgsnd(out_msgid, &message, sizeof(message), IPC_NOWAIT) == -1)
	  perror("SERVER: Error ");
	else {
	  //printf("SERVER: Response sent.\n");
	}

	break;
      }
    case READ_DATA:
      {
	//printf("SERVER: Received READ_DATA message...\n");
	message.msg_type = READ_DATA_OK;
	memcpy(message.msg_text, avr->data, SRAM_BYTES);

	if (msgsnd(out_msgid, &message, sizeof(message), IPC_NOWAIT) == -1)
	  perror("SERVER: Error ");
	else {
	  //printf("SERVER: Response sent.\n");
	}
	
	break;
      }
    case WRITE_DATA:
      {
	//printf("SERVER: Received WRITE_DATA message...\n");

	memcpy(avr->data, message.msg_text, SRAM_BYTES);
	
	message.msg_type = WRITE_DATA_OK;

	if (msgsnd(out_msgid, &message, sizeof(message), IPC_NOWAIT) == -1)
	  perror("SERVER: Error ");
	else {
	  //printf("SERVER: Response sent.\n");
	}
	
	break;
      }
    case STEP:
      {
	//printf("SERVER: Received STEP message...\n");
	step(266666);

	message.msg_type = STEP_OK;
	memcpy(message.msg_text, avr->data, SRAM_BYTES);

	if (msgsnd(out_msgid, &message, sizeof(message), IPC_NOWAIT) == -1)
	  perror("SERVER: Error ");
	else {
	  //printf("SERVER: Response sent.\n");
	}
	
	break;
      }
    case KILL:
      {
	printf("SERVER: Received KILL message...\n");

	message.msg_type = KILL_OK;

	if (msgsnd(out_msgid, &message, sizeof(message), IPC_NOWAIT) == -1)
	  perror("SERVER: Error ");
	else {
	  printf("SERVER: Response sent.\n");
	}
	  
	kill_requested = 1;
	break;
      }
    case INIT_OK:
    case INIT_FAILED:
    case READ_DATA_OK:
    case WRITE_DATA_OK:
    case STEP_OK:
    case KILL_OK:
      printf("SERVER: Received invalid message type: %u\n", t);
      break;
  }

    // Sleep
  }

  // to destroy the message queue
  printf("SERVER: Destroying message queue...\n");
  msgctl(in_msgid, IPC_RMID, NULL);
  msgctl(out_msgid, IPC_RMID, NULL);

  printf("SERVER: Exiting...\n");
  return 0;
}

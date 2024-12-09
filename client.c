/*
2010205537--muhenned sıffu
2110205525--Halit abdulrauf
2010205553--Beraa ceze 
2210205310--Mustaf Jhar

*/
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#define PORT 9013
#define NAME_LENGTH 32
#define MESSAGE_LENGTH 128
#define BUFFER_LENGTH 256
#define MAX_NUM_OF_CLIENTS 10

typedef struct MESG { //message command
    char command[10];
    char message[MESSAGE_LENGTH];
    char crc[33];
    char from[NAME_LENGTH];
    char to[NAME_LENGTH];
    char parity;

} MESG;

typedef struct MERR { //bad message command
    char command[5];
    char from[NAME_LENGTH];
    char to[NAME_LENGTH];

} MERR;

typedef struct CONN { //connection command
    char command[5];
    char name[NAME_LENGTH];
} CONN;

void message_to_string(MESG* msg, char buffer[]){
    sprintf(buffer, "%s|%s|%s|%s|%s|%c", msg->command, msg->to, msg->from, msg->message, msg->crc, msg->parity);
}
void connection_to_string(CONN* connection, char buffer[]){
    sprintf(buffer, "%s|%s", connection->command, connection->name);
}
void message_error_to_string(MERR* msg, char buffer[]){
    sprintf(buffer, "%s|%s|%s", msg->command, msg->to, msg->from);
}
void string_to_message(char buffer[], MESG* msg){
    char copy[BUFFER_LENGTH];
    strcpy(copy, buffer);
    strcpy(msg->command,strtok(copy, "|"));
    strcpy(msg->to, strtok(NULL, "|"));
    strcpy(msg->from, strtok(NULL, "|"));
    strcpy(msg->message, strtok(NULL, "|"));
    strcpy(msg->crc, strtok(NULL, "|"));
    msg->parity = strtok(NULL, "|")[0];
}


static uint32_t crc32LookupTable[256];

FILE* logFile;
int exitCommand = 0;
char name[NAME_LENGTH];
int socketId;
//used to speed up CRC calculations for data integrity checks
void make_crc_table() {
    uint32_t POLYNOMIAL = 0xEDB88320;
    uint32_t remainder;
    unsigned char b = 0;
    do {
        // Start with the data byte
        remainder = b;
        for (uint32_t bit = 8; bit > 0; --bit) {
            
            if (remainder & 1)
                remainder = (remainder >> 1) ^ POLYNOMIAL;
            else
                remainder = (remainder >> 1);
        }
        crc32LookupTable[(size_t)b] = remainder;
    } while(0 != ++b);
}
/*It takes an array of characters (data), 
the length of the data (data_len), 
and an array of characters (bits) to store the resulting CRC-32 value in binary representation */
void CRC32(char data[], size_t data_len, char bits[]) {
	uint32_t crc32 = 0xFFFFFFFFu;//Initializes the CRC-32 variable with the initial CRC value (0xFFFFFFFF)
	int i;
	for (i=0; i < data_len; i++) {
		uint32_t Index = (crc32 ^ data[i]) & 0xff;
		crc32 = (crc32 >> 8) ^ crc32LookupTable[Index];  
	}
	crc32 ^= 0xFFFFFFFFu;
    
    for(i = 0; i < 32; i++){
        bits[i] = '0' + ((crc32 >> i) & 0x0001);
    }
    bits[i] = 0;
}
//calculates the parity bit for a given array of characters
char get_parity_bit(char data[], size_t data_length){
    int Count = 0;
    int i;
    for(i = 0; i < data_length; i++){
        int j;
        for(j = 0; j < sizeof(char)*8; j++){
            if((data[i] >> j) & 1){
                Count++;
            }
        }
    }
    return '0' + Count%2;
}


void ctrl_c_and_exit(int s) {
    exitCommand = 1;
}
void message_sender(){
    while(1){
        char message[MESSAGE_LENGTH+NAME_LENGTH];
        char messageCpy[MESSAGE_LENGTH+NAME_LENGTH];
        char msgBuffer[BUFFER_LENGTH];
        
        struct MESG* msg = (MESG*) malloc (sizeof(MESG)); //The message variable is created.

        fgets(message, MESSAGE_LENGTH+NAME_LENGTH, stdin);
        strcpy(messageCpy, message);
        char* to = strtok(messageCpy, "->");//tak the strıng to reach -> 
        char* text = strtok(NULL, "->");//from the previous strtok ignore -> and take the string to reach ->
        if(text == NULL)//if meassge is null
        {
            to[strcspn(to, "\n")] = 0;//removes the newline character (if any) from the to string
            if(strcmp(to, "logout") == 0){
                char goneMessage[5] = "GONE";
                send(socketId, goneMessage, strlen(goneMessage), 0);
                exitCommand = 1;
                break;
            }
            else{
                fprintf(stdout, "Unknown operation\n");
                continue;
            }
        }
        //mesg not null
        strcpy(msg->command, "MESG"); // message command is set.
        text[strcspn(text, "\n")] = 0;//remove the new line after \n 
        strcpy(msg->message, text); //message text is set
        strcpy(msg->to, to); //It is determined who it will go to.
        strcpy(msg->from, name); //Who sent it is set.
        CRC32(msg->message, sizeof(msg->message), msg->crc);//crc bits are set.
        msg->parity = get_parity_bit(msg->message, sizeof(msg->message)); // The parity bit is set/found.

        message_to_string(msg, msgBuffer); //Converts it into a single string (|)
        
        send(socketId, msgBuffer, strlen(msgBuffer), 0); //The message is sent to the server.
        fprintf(logFile, "%s\n", message); //log file is written
        free(msg);
    }
    pthread_detach(pthread_self());
}
void ReceiveChat() { //works when message arrives
    char message[BUFFER_LENGTH];
    while (1) {
		int receive = recv(socketId, message, BUFFER_LENGTH, 0); //The line where the message comes from
        if (receive > 0) { //presence of message
            char copy[BUFFER_LENGTH];
            strcpy(copy, message); 
            char* CMD = strtok(copy, "|"); //reads the command part of the message
            char* token = strtok(NULL, "|"); //reads the rest of the message
            if(token == NULL){ //If it consists of only commands
                fprintf(stdout, "%s\n", message); //prints the entire message
            }
            if(strcmp(CMD, "MESG") == 0){ //If a message command arrived
                struct MESG* msg = (MESG*)malloc(sizeof(MESG)); //message variable is created
                string_to_message(message, msg); //message is put in msg
                char currCRC[33]; 
                CRC32(msg->message, sizeof(msg->message), currCRC); //The crc value of the incoming message is calculated
                char parity = get_parity_bit(msg->message, sizeof(msg->message)); //The parity value of the incoming message is calculated
                
                if(strcmp(msg->crc, currCRC) != 0 || msg->parity != parity){ //If one of the crc or parity values is different
         
                fprintf(stdout,"message crc:%s\n current crc:%s\n",msg->crc, currCRC); //It shows the required and calculated crcs.
                	
                    struct MERR* err = (MERR*)malloc(sizeof(MERR));
                    strcpy(err->command, "MERR");
                    strcpy(err->from, msg->from);
                    strcpy(err->to, msg->to);
                    char errMessage[BUFFER_LENGTH];
                    message_error_to_string(err, errMessage); 
                    send(socketId, errMessage, strlen(errMessage), 0); //incorrect message command is sent
                }
                else{ //If the message is not incorrect, the logo and the message will be displayed on the screen.
                    fprintf(stdout, "%s:%s\n", msg->from, msg->message);
                    fprintf(logFile, "%s:%s\n", msg->from, msg->message);
                }
            }
        }  
        else if (receive == 0) {
			break;
        }
        else {
            // -1
		}
		memset(message, 0, BUFFER_LENGTH);
    }
    pthread_detach(pthread_self());
}
int main(){ 
    printf("Enter your chat name: ");
    fgets(name,NAME_LENGTH,stdin); //username is taken
    name[strcspn(name, "\n")] = 0; //Deletes the \n character from the end of the string
    make_crc_table(); //crc table is created
   
    struct sockaddr_in server_addr;
   
    pthread_t messageSenderThread;
   
    pthread_t receiveChatThread;
//creat socket
    socketId = socket(AF_INET, SOCK_STREAM,0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    int connection = connect(socketId, (struct sockaddr*) &server_addr, sizeof(server_addr));
    if (connection == -1) {
    perror("Error connecting to the server");
    exit(EXIT_FAILURE);}
//	int connection = connect(socketId, (struct server_addr*) &server_addr, sizeof(server_addr));
    CONN* connectionCommand = (CONN*)malloc(sizeof(CONN));
    strcpy(connectionCommand->command, "CONN");
    strcpy(connectionCommand->name, name);
    char connectionMessage[BUFFER_LENGTH];
    connection_to_string(connectionCommand, connectionMessage);
    send(socketId, connectionMessage, BUFFER_LENGTH, 0);
    char message[BUFFER_LENGTH];
    recv(socketId, message, BUFFER_LENGTH, 0);
    printf("%s\n", message);
    struct stat info; //Otherwise Log folder will be created
    char dirName[] = "./Logs";
    if( stat( dirName, &info ) != 0 ){
        mkdir(dirName, 0700);
    }
    
    time_t t = time(NULL); //log file is created
    struct tm tm = *localtime(&t);
    char fileName[128];
    sprintf(fileName,"./Logs/%s-%d-%d-%d-%d-%d.txt", name, tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,tm.tm_hour,tm.tm_min);
    fprintf(stdout, "%s\n",fileName);
    logFile = fopen(fileName, "w+");
    //Sender and receiver threads are initiated
    pthread_create(&messageSenderThread, NULL, (void*)&message_sender, NULL);
    pthread_create(&receiveChatThread, NULL, (void*)&ReceiveChat, NULL);

    //Set exit flag when ctrl+c received from keyboard
    signal(SIGINT, ctrl_c_and_exit);
    //message_sender();
    while(1){
        if(exitCommand){
            printf("Closing connection.\n");
            break;
        }
        sleep(1);
    }
    fclose(logFile);
}

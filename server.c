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


typedef struct clientInfo{
    int id;
    struct sockaddr_in address;
    char name[NAME_LENGTH];
    int socketId;
} clientInfo;

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
//sprintf fun from ,<stdio.h> library 
/*
The sprintf function works similarly to printf,
 but instead of printing the formatted text to the standard output (usually the console),
  it writes the formatted text into the character array specified by the first parameter
*/
void message_to_string(MESG* msg, char buffer[]){
    sprintf(buffer, "%s|%s|%s|%s|%s|%c", msg->command, msg->to, msg->from, msg->message, msg->crc, msg->parity);
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

void string_to_connection(char buffer[], CONN* connection){
    char copy[BUFFER_LENGTH];
    strcpy(copy, buffer);
    strcpy(connection->command,strtok(copy, "|"));
    strcpy(connection->name, strtok(NULL, "|"));
}

void string_to_message_error(char buffer[], MERR* msg){
    char copy[BUFFER_LENGTH];
    strcpy(copy, buffer);
    strcpy(msg->command,strtok(copy, "|"));
    strcpy(msg->to, strtok(NULL, "|"));
    strcpy(msg->from, strtok(NULL, "|"));
}


int clientCount = 0;
int exitCommand = 0; //exit flag
clientInfo* clientList[MAX_NUM_OF_CLIENTS]; //keeps a list of users
char latestMessagesFromTo[MAX_NUM_OF_CLIENTS][MAX_NUM_OF_CLIENTS][BUFFER_LENGTH]; //List of last messages sent by person x to someone else
int availableIds[MAX_NUM_OF_CLIENTS];  //obtainable ids

int getAvailableId(){  //Returns the first retrievable id
    int i;
    for(i = 0; i < MAX_NUM_OF_CLIENTS; i++){
        if(availableIds[i]){
            return i;
        }
    }
    return -1;
}

void sendJoinedChatMessage(int id,char name[]){ //It sends a message to the people in the chat that the new person has joined.
    int i;
    
    char buffer[MESSAGE_LENGTH];
    sprintf(buffer, "%s joined to chat", name);//copy the text and put inthe buffer
    for(i = 0; i < MAX_NUM_OF_CLIENTS; i++){
        if(clientList[i] == NULL) continue;
        if(id != clientList[i]->id){
            write(clientList[i]->socketId, buffer, strlen(buffer));//function to send data over a socket
           
        }
    }
}

void sendLeftChatMessage(int id,char name[]){ //Sends a message to people in the chat that the person has logged out.
    int i;
    char buffer[MESSAGE_LENGTH];
    sprintf(buffer, "%s left the chat", name);
    for(i = 0; i < MAX_NUM_OF_CLIENTS; i++){
        if(clientList[i] == NULL) continue;
        if(id != clientList[i]->id){
            write(clientList[i]->socketId, buffer, strlen(buffer));
            
        }
    }
}
void getClientNameList(int id, char buffer[]){ //Returns a list of names of people in the chat
    if(clientCount == 1){
        strcpy(buffer,"You are the first person on the chat!");
        return;
    }
    strcpy(buffer, "---USER LIST---\n");
    int i;
    for(i = 0; i < MAX_NUM_OF_CLIENTS; i++){
        if(clientList[i] == NULL) continue;
        if(clientList[i]->id == id) continue;
        buffer = strcat(buffer, " - ");
        buffer = strcat(buffer, clientList[i]->name);
        buffer = strcat(buffer, "\n");
    }
}
void *ServiceClient(void* client){   //Executes user operations.x
    char buffer[BUFFER_LENGTH];
    srand(time(NULL));
    
    clientInfo* client_info = (clientInfo*)client;
    if(recv(client_info->socketId, buffer, BUFFER_LENGTH, 0) > 0){ //first contact
        CONN* conn = (CONN*)malloc(sizeof(CONN));
        string_to_connection(buffer, conn);
        strcpy(client_info->name, conn->name);

        char joinMessage[MESSAGE_LENGTH];
        sprintf(joinMessage, "%s joined to chat", client_info->name);
       
        sendJoinedChatMessage(client_info->id, client_info->name); //joining information
        char clientListMessage[BUFFER_LENGTH];
		memset(clientListMessage, 0, BUFFER_LENGTH);

        getClientNameList(client_info->id, clientListMessage); //contact list comes
        if(write(client_info->socketId, clientListMessage, sizeof(clientListMessage)) < 0){ //contact list is sent to the client
            perror("ERROR: write to descriptor failed");
        }
    }
    
    int toId;
    while(!exitCommand){ //listening mode
        memset(buffer, 0, BUFFER_LENGTH);
        if(recv(client_info->socketId, buffer, BUFFER_LENGTH, 0) > 0){ //receives message from client
            if(strcmp(buffer, "q") == 0){ //the connection is terminated.
                break;
            }
            else{ 
                char copy[BUFFER_LENGTH];
                strcpy(copy, buffer);
                char* command = strtok(copy,"|"); //command information is received
                if (strcmp(command, "MESG") == 0){
                    MESG* message = (MESG*)malloc(sizeof(MESG));
                    string_to_message(buffer, message);
                    if(rand()%2){ //It is decided whether an error will be added to the message or not.
                        fprintf(stdout, "Generating noise\n");
                        //number of characters to distort, maximum 1/3 of the message length
                        int charCount = rand()%(sizeof(message->message)/3);
                        int i;
                        for(i = 0; i < charCount; i++){ //random characters of the message are replaced
                            int index = rand()%sizeof(message->message);
                            char newChar = 'a' + rand()%26;
                            message->message[index] = newChar;
                        }
                    }
                    
                    for(toId = 0; toId < clientCount; toId++){ //The ID of the person to whom the message will be sent is found.
                        if(strcmp(message->to, clientList[toId]->name) == 0) break;
                    }
                    strcpy(latestMessagesFromTo[client_info->id][toId],buffer); // The intact version of the message is saved as the last message sent to the contact
                    char sendMessage[BUFFER_LENGTH];
                    message_to_string(message, sendMessage);
                    write(clientList[toId]->socketId, sendMessage, sizeof(sendMessage)); //The message is sent to the relevant person.
                }
                else if(strcmp(command, "MERR") == 0){ //When the bad message command arrives
                    MERR* merr = (MERR*) malloc(sizeof(MERR));
                    string_to_message_error(buffer, merr);
                    for(toId = 0; toId < clientCount; toId++){ //The ID of the person to whom the erroneous message was sent is found
                        if(strcmp(merr->to, clientList[toId]->name) == 0) break;
                    }
                    int fromId;
                    for(fromId = 0; fromId < clientCount; fromId++){ //The ID of the person who sent the incorrect message is found.
                        if(strcmp(merr->from, clientList[fromId]->name) == 0) break;
                    }

                    fprintf(stdout, "MESSAGE ERROR DETECTED\nMessage: %s\n",latestMessagesFromTo[fromId][toId]);
                    write(clientList[toId]->socketId, latestMessagesFromTo[fromId][toId], sizeof(latestMessagesFromTo[fromId][toId])); //son yollanan mesajın hatasız hali gonderilir. 
                    memset(latestMessagesFromTo[fromId][toId], 0, BUFFER_LENGTH);
                }
                else if(strcmp(command, "GONE") == 0){ //If the exit command came
                    sendLeftChatMessage(client_info->id, client_info->name); //An exit message is sent to users in the chat.
//                    fprintf(stdout, "%s joined to chat", client_info->name);
                    break;
                }
            }
        }
        else{
            break;
        }
    }
    //EXIT PERSON PROCEDURES
    clientCount--;
    availableIds[client_info->id] = 1;
    clientList[client_info->id] = NULL;
    close(client_info->socketId);
    free(client_info);
    pthread_detach(pthread_self()); //thread is closed
    return NULL;
}

void ctrl_c_and_exit(int sig) {
    exitCommand = 1;
}
int main(){
     printf("server is workeing ");
    int index;
    for(index = 0; index < MAX_NUM_OF_CLIENTS; index++){ //All ids become available.
        availableIds[index] = 1;
        clientList[index] = NULL; //client list is reset
    }
    //SOKET OLUŞTURMA İŞLEMLERİ
    int listenId = 0;
    int connectionId = 0;
    memset(clientList, 0, MAX_NUM_OF_CLIENTS);//حجز مكان بالذاكر للمصفوفة 
    /*
    void *memset(void *s, int c, size_t n);
s: A pointer to the memory block to be filled.
c: The value to be set (as an unsigned char).
n: The number of bytes to be set to the value.
    */
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    
    pthread_t threadId;

    listenId = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    /*htons is used to convert the port number from host byte order to network byte order*/

    if(bind(listenId, (struct sockaddr*) &server_addr, sizeof(server_addr))){
        perror("ERROR: Socket binding failed");
        return -1;
    }

    if(listen(listenId, MAX_NUM_OF_CLIENTS)< 0){
         perror("ERROR: Socket listening failed");
        return -1;
	}


    signal(SIGINT, ctrl_c_and_exit);
    while(!exitCommand)
	{ //handles incoming clients
        int size = sizeof(client_addr);
        connectionId = accept(listenId, (struct sockaddr*)&client_addr, &size);
		
        clientInfo* client_info = (clientInfo*)malloc(sizeof(clientInfo));
        client_info->socketId = connectionId;
        client_info->address = client_addr;
        client_info->id = getAvailableId();
        availableIds[client_info->id] = 0;
        clientList[clientCount] = client_info;
        clientCount++;
         
        pthread_create(&threadId, NULL, &ServiceClient, (void*)client_info);
        sleep(1);
    }
    

}

/*
 * File name: proxyserver.c
 * Project: HTML Server Assignment 7
 * CPSC 333
 * Author: @Rakan AlZagha
 * Comments: Used telnet format for client input!
 */

#define SERVER_PORT 9555
#define PROXY_PORT 9111
#define BUFFERSIZE 8192

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>

void handle_ctrlc(int sig);
int control_socket, data_socket, fileDescriptor;

/*
 *   Function: main()
 *   ----------------------------
 *   Purpose: wait for connection from client and return appropriate messages from original server
 *
 */

int main(int argc, char *argv[]) {
    struct sockaddr_in server_control_address; //socket address for server
    struct sockaddr_in client_address; //socket address for client
    struct hostent *hostConvert;       //convert hostname to IP
    struct sockaddr_in server_data_address;
    struct dirent *directories;
    char data_buffer[BUFFERSIZE];            //buffer of size 8192
    char success[BUFFERSIZE];            //buffer of size 8192
    int i, n, file, bind_override = 1, address_size, error_check;
    char command[50], filename[255], filename_temp[50], temp_command[50];
    char fileNameTemp[255];
    char* success_message = "HTTP/1.1 200 OK\n\n";
    char* error_message = "HTTP/1.1 404 Not Found\n\n<html><head></head><html><body><h1>404 Not Found</h1></body></html>\n";
    char* command_error = "COMMAND NOT FOUND\n";
    char* client_command;
    char* file_token;
    char* host_token;
    char* port_token;
    char* ip;
    char* hostname;
    int validCommand = -1;
    int fileInCache = 0;

    signal(SIGINT, handle_ctrlc);

    address_size = sizeof(client_address); //establishing the size of the address

    //socket structures set to null
    bzero(&client_address, sizeof(client_address));
    bzero(&server_control_address, sizeof(server_control_address));

    //control_socket implementation
    control_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(control_socket < 0){ //error checking
        perror("Failed to create socket, please try again!\n");
        exit(1);
    }
    if (setsockopt(control_socket, SOL_SOCKET, SO_REUSEADDR, &bind_override, sizeof(int)) < 0) //bind kept failing after repeated use, SO_REUSEADDR fixed the problem
      perror("Reusable socket port failed...try again.\n");

    //server ipv4, server port, and IP address
    server_control_address.sin_family = AF_INET;
    server_control_address.sin_port = htons(SERVER_PORT);
    server_control_address.sin_addr.s_addr = htonl(INADDR_ANY);

    //assign the address specified by addr to the socket referred to by the file descriptor
    error_check = bind(control_socket, (struct sockaddr *)&server_control_address, sizeof(server_control_address));
    if(error_check < 0){ //error checking
        perror("ERROR at bind function!\n");
        exit(1);
    }

    error_check = listen(control_socket, 1); //listen
    if(error_check < 0){ //error checking
        perror("ERROR at listen function!\n");
        exit(1);
    }

    fileDescriptor = accept(control_socket, (struct sockaddr *)&client_address, &address_size); //accept connection request
    if(fileDescriptor < 0){ //error checking
        perror("ERROR at accept function!\n");
        exit(1);
    }
    else{
        printf("Accepted connection from client.\n.\n.\n.");
    }

    while(1){
        validCommand = -1;
        bzero(filename, sizeof(filename)); //set filename to null
        bzero(command, sizeof(command)); //set filename to null
        read(fileDescriptor, command, sizeof(command)); //read the command from the client

        client_command = strtok(command, " "); //extract the command (first token)
        if(strcmp(client_command, "GET") == 0){
            validCommand = 0;
        }
        switch (validCommand){
            case 0:
                printf("\nCommand accepted.\n");
                host_token = strtok(NULL, "/:"); //extract the name of the host (2nd token)
                port_token = strtok(NULL, ":/"); //extract the port of the host (3rd token)
                file_token = strtok(NULL, "/"); //extract the name of the file (4th token)
                strcpy(filename_temp, file_token); //copy into char array from char * context
                for(i = 0; i < sizeof(filename_temp); i++){ //removing random characters and null from end of filename
                    if(i == strlen(filename_temp) - 2) //there are 2 random character at the end of the filename
                        break;
                    filename[i] = filename_temp[i]; //copy temp to real filename
                }

                DIR *directory_stream = opendir("."); //"." signifies the current directory

                if (directory_stream != NULL) {
                    while ((directories = readdir(directory_stream)) != NULL){ //while there are more filenames to read
                        strncpy(fileNameTemp, directories->d_name, 254); //copy the name of the file into the struc
                        fileNameTemp[254] = '\0'; //nullify the filename (clear)
                        if(strncmp(fileNameTemp, filename, sizeof(fileNameTemp)) == 0){ //if we encounter the filename
                            fileInCache = 1; //return 1 (true)
                            break;
                        }
                        else{
                            fileInCache = 0; //return 0 (false)
                        }
                    }
                    closedir(directory_stream); //close the directory that was opened
                }

                else { // directory_stream == NULL
                    printf("Could not open current directory" );
                    return 0;
                }

                if(fileInCache == 1) {
                    printf("File found in cache.\n");
                    if((file = open(filename, O_RDONLY, S_IRUSR)) < 0){ //open local file to read
                        write(fileDescriptor, error_message, strlen(error_message)); //return error if file not found
                        perror("ERROR in opening\n");
                        close(fileDescriptor);
                        continue;
                    }
                    while((n = read(file, data_buffer, BUFFERSIZE)) > 0){ //read local file
                        data_buffer[n] = '\0'; //set buffer to NULL prior to writing
                        if(write(fileDescriptor, data_buffer, n) < n){ //write date over to the server
                            perror("ERROR in writing\n");
                            exit(1);
                        }
                        else{
                            break;
                        }
                    }
                }

                else {
                    int data_address_size = sizeof(server_data_address);
                    hostname = host_token; //extracting the name of the host we are connecting to (first argument)
                    hostConvert = gethostbyname(hostname); //converting name to host address
                    if(hostConvert == NULL){ //error checking that host is valid (ex. syslab001... would error)
                        printf("Not a valid hostname. Please try again.\n");
                        exit(1);
                    }

                    ip = inet_ntoa(*(struct in_addr *) *hostConvert->h_addr_list); //converts host address to a string in dot notation

                    bzero(&server_data_address, sizeof(server_data_address));
                    data_socket = socket(AF_INET, SOCK_STREAM, 0);
                    if(data_socket < 0){ //error checking
                        perror("Failed to create data socket, please try again!\n");
                        exit(1);
                    }

                    int port = atoi(port_token);

                    //connecting to original server specifications
                    server_data_address.sin_family = AF_INET;
                    server_data_address.sin_port = htons(port);
                    server_data_address.sin_addr.s_addr = inet_addr(ip);

                    //connect
                    error_check = connect(data_socket, (struct sockaddr *)&server_data_address , data_address_size);
                    if (error_check != 0){ //error checking
                        perror("ERROR in connect function\n");
                        exit(1);
                    }
                    else{
                        printf("Successfully connected to original server.\n");
                    }

                    //combining the client command and filename
                    char* server_command = (char *) malloc(1 + strlen(client_command) + strlen(file_token) + strlen(" /"));
                    strcpy(server_command, client_command);
                    strcat(server_command, " /");
                    strcat(server_command, file_token);

                    //writing to server the command
                    write(data_socket, server_command, sizeof(command));
                    printf("HERE 1");

                    if((file = open(filename, O_WRONLY|O_CREAT, S_IRWXU)) < 0){ //create and or write privelages
                        perror("ERROR in opening\n");
                        exit(1);
                    }

                    while((n = read(data_socket, success, BUFFERSIZE)) > 0){ //read data in
                        success[n] = '\0';
                        write(fileDescriptor, success, n); //write to the socket
                    }

                    while((n = read(data_socket, data_buffer, BUFFERSIZE)) > 0){ //read data in
                        data_buffer[n] = '\0';
                        write(file, data_buffer, n); //write to the file
                        write(fileDescriptor, data_buffer, n); //write to the socket
                    }
                }
            break;

            default:
                printf("\nCommand unknown!\n");
                continue;
            break;
        }
    }
}

/*
 *   Function: handle_ctrlc()
 *   ----------------------------
 *   Purpose: handle ctrl-c command
 *
 *   int sig
 *
 */

void handle_ctrlc(int sig) {
   fprintf(stderr, "\nCtrl-C pressed\n");
   close(data_socket);
   close(control_socket);
   close(fileDescriptor);
   exit(0);
}

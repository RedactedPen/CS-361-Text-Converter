#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include "big_money_ne.h"


#define PORT "54325"


int char_to_index(char c){
    if(64 < c && c < 91){
        return c - 63;
    }else if(96 < c && c < 123){
        return c - 95;
    }else{
        return 0;
    }
}

char* convert(int font, char* text){
    if(font != 1){
        return NULL;
    }

    int len = strlen(text);
    char* converted = malloc(sizeof(char) * ((BIG_MONEY_NE_HEIGHT * BIG_MONEY_NE_WIDTH + 1) * len));
    converted[0] = '\0';
    for(int i = 0; i < BIG_MONEY_NE_HEIGHT; i++){
        for(int j = 0; j < len; j++){
            int index = char_to_index(text[j]);
            strcat(converted, big_money_ne[index][i]);
        }
        strcat(converted, "\n");
    }
    
    return converted;
}



//Creates the socket for connecting to clients
//Returns the socket on a success, or a -1 on a failure
int create_socket(){
    //Set up addr list
    struct addrinfo* addr_list = NULL;

    //Set up hints
    struct addrinfo hints = {0};
    //Set hints to use IPv6
    hints.ai_family = AF_INET6;
    //Set hints to use TCP
    hints.ai_socktype = SOCK_STREAM;
    //Set hints to use the default TCP protocol
    hints.ai_protocol = 0;

    //Get the address info for localhost
    int get_addr_result = getaddrinfo(
        "localhost", 
        PORT, 
        &hints, 
        &addr_list
    );

    //Check that the function worked
    if(get_addr_result != 0){
        fprintf(stderr, "Error code %i on getaddrinfo. Exiting\n", get_addr_result);
        return -1;
    }

    //Set up the listening socket using IPv6 and the default TCP prototcol
    int listen_socket = socket(AF_INET6, SOCK_STREAM, 0);

    //Attempt to bind to the address(es) returned by getaddrinfo
    int bind_result = -1;
    struct addrinfo* addr = addr_list;
    while(addr != NULL && bind_result == -1){
        //Attempt to bind to the address held by addr
        bind_result = bind(
            listen_socket,
            addr->ai_addr,
            addr->ai_addrlen
        );
        addr = addr->ai_next;
    }

    //Check if the binding process failed
    if(bind_result == -1){
        fprintf(stderr, "Failed to bind to localhost. Exiting \n");
        return -1;
    }

    if(listen_socket < 0){
        return -1;
    }

    int listen_result = listen(listen_socket, 1);
    if(listen_result == -1){
        fprintf(stderr, "Failed to listen to the port. Exiting\n");
        return -1;
    }

    return listen_socket;
}

//Recieves a message from the socket. Stores the message in the provided message pointer
//Returns 0 on success, -1 on a failure
int recieve_message(int socket, char** message){
    size_t peek_size = 1025;
    char* peek_text = (char* ) malloc(sizeof(char) * peek_size);
    int total_bytes_recieved = 0;
    int max_bytes_remaining = peek_size;

    char eot[2] = {4, '\0'};

    //Determines the length of text to read
    while(strstr(peek_text, eot) == NULL){
        if(max_bytes_remaining == 0){
            max_bytes_remaining += peek_size;
            peek_size *= 2;
            peek_text = (char *) realloc(peek_text, peek_size);
        }

        //Recieve data from the client
        int bytes_recieved = recv(
            socket,
            peek_text + total_bytes_recieved,
            max_bytes_remaining,
            MSG_PEEK
        );

        if(bytes_recieved > 0){
            //Client sent data, record how much was recieved
            total_bytes_recieved += bytes_recieved;
            max_bytes_remaining -= bytes_recieved;
        }else if(bytes_recieved == 0){
            //Client sent no data
            return -1;
        }else{
            //recv had an error
            fprintf(stderr, "recieve_message: Error receiving bytes, recv returned %i\n", bytes_recieved);
            return -1;
        }
    }

    char* transmission_end = strstr(peek_text, eot);
    transmission_end[0] = '\0';

    *message = peek_text;
    return 0;
}

int transmit(int socket, char* data){
    //Set up the variables for sending data
    int total_bytes_sent = 0;
    int bytes_to_send = strlen(data);
    int bytes_remaining = bytes_to_send;

    //Loop until all bytes have been sent
    while(total_bytes_sent < bytes_to_send){
        //Send the message
        int bytes_sent = send(
            socket, 
            data + total_bytes_sent,
            bytes_remaining,
            0
        );

        if(bytes_sent != -1){
            total_bytes_sent += bytes_sent;
            bytes_remaining -= bytes_sent;
        }else{
            //Some error occured. Print an error message
            fprintf(stderr, "transmit: Error on sending data\n");
            return -1;
        }
    }
    return 0;
}

//Parses the provided request
//Stores the text part of the request in text
//Returns the font number requested
//WARNING: Alters the request string
int parse_request(char* request, char** text){
    //Find the delimiter between the font request and the text request
    char* delimiter = strchr(request, ':');
    if(delimiter == NULL){
        printf("Client sent malformed request '%s'", request);
        return -1;
    }

    //Set the text pointer to point to the base of the text
    *text = delimiter + 1;

    //Parse the font request
    delimiter[0] = '\0';
    int font_req = atoi(request);
    if(font_req == 0){
        printf("Unable to parse font request '%s'", request);
        return -1;
    }

    return font_req;
}

//Handles processing and responding to a client's request
void handle_client(int socket){
    //Get the request that the client is sending
    char* request;
    int result = recieve_message(socket, &request);
    if(result < 0){
        printf("Failed to recieve data from client\n");
        //Shutdown the socket and exit the function if an error occured
        shutdown(socket, SHUT_RDWR);
        return;
    }

    printf("Recieved request '%s'\n", request);

    //Parse the request that was recieved from the client
    char* text;
    int font = parse_request(request, &text);

    //Convert the text as the client requested
    char* converted_text = convert(font, text);

    //Prepare the text for transmission
    char eot[2] = {4, '\0'};
    strcat(converted_text, eot);

    printf("Responding with converted text\n%s\n", converted_text);

    //Reply with the converted text
    transmit(socket, converted_text);

    free(request);
    free(converted_text);
    shutdown(socket, SHUT_RDWR);
}



int main(){


    //Set up the communication socket
    int socket = create_socket();
    if(socket < 0){
        return 1;
    }
    
    printf("Waiting for requests... \n");
    while(1){
    
        //Set up structures to connect to a client
        struct sockaddr_in client_addr;
        socklen_t client_addr_size = sizeof(client_addr);

        //Accept the connection from the client
        int client_socket = accept(socket, (struct sockaddr*) &client_addr, &client_addr_size);
        printf("Connected to client\n");

        //Check that the connection was succesful
        if(client_socket == -1){
            fprintf(stderr, "Failed to accept client request\n");
            return 1;
        }

        //Process and respond to the client's request
        handle_client(client_socket);
    }

}
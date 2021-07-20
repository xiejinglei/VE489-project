// TODO: implement ftrans here 
#include <stdio.h>		// printf(), perror()
#include <stdlib.h>		// atoi()
#include <sys/socket.h>		// socket(), bind(), listen(), accept(), send(), recv()
#include <unistd.h>		// close(), stderr
#include <string.h>		// memcpy()
#include <arpa/inet.h>		// htons(), ntohs()
#include <netdb.h>		// gethostbyname(), struct hostent
#include <netinet/in.h>		// struct sockaddr_in

static const size_t MAX_MESSAGE_SIZE = 256;

/**
 * Endlessly runs a server that listens for connections and serves
 * them _synchronously_.
 *
 * Parameters:
 *		port: 		The port on which to listen for incoming connections.
 *		queue_size: 	Size of the listen() queue
 * Returns:
 *		-1 on failure, does not return on success.
 */
int run_server(int port, int queue_size);

/**
 *	hostname: 	Remote hostname of the server.
 *	port: 		Remote port of the server.
 * 	message: 	The message to send, as a C-string.
 *  client_port: The port that client binds to.
 */
int run_client(const char *hostname, int port, const char *message, int client_port);

/**
 *	The server sends a file's length and then the file to client
 *	sockfd: file descriptor for socket
 *  filename: the name of file
 */
void sendFile(int sockfd, const char *filename);

/**
 *	The client receives a file
 *	sockfd: file descriptor for socket
 *  filename: the name of file
 */
void recvFile(int sockfd, const char* filename);


int main(int argc, const char **argv)
{
    // Parse command line arguments
	if (argc != 4 && argc != 10) {
		printf("As server: ./ftrans -s -p <port>\n");
        printf("As client: ./ftrans -c -h <server’s IP/hostname> -sp <server port> -f <filename> -cp <client port>\n");
		return 1;
	}

    // Server mode
    if (argc == 4)
    {
        int port = atoi(argv[3]);
        if (run_server(port, 10) == -1) {
		    return 1;
	    }
    }

    // Client mode
    if (argc == 10)
    {
        const char *hostname = argv[3];
        int port = atoi(argv[5]);
        const char *filename = argv[7];
	    int client_port = atoi(argv[9]);
	    
        //printf("Sending message %s to %s:%d\n", filename, hostname, port);
	    if (run_client(hostname, port, filename, client_port) == -1) {
		    return 1;
	    }
    }


    return 0;
}



int run_server(int port, int queue_size)
{
    // (1) Create socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("Error opening stream socket");
		return -1;
	}

    // (2) Set the "reuse port" socket option
	int yesval = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yesval, sizeof(yesval)) == -1) {
		perror("Error setting socket options");
		return -1;
	}

    // (3) Create a sockaddr_in struct for the proper port and bind() to it.
	struct sockaddr_in addr;

    // 3(a) Make sockaddr_in
	addr.sin_family = AF_INET;
	// Let the OS map it to the correct address.
	addr.sin_addr.s_addr = INADDR_ANY;
	// If port is 0, the OS will choose the port for us. Here we specify a port.
	// Use htons to convert from local byte order to network byte order.
	addr.sin_port = htons(port);
	// Bind to the port.
	if (bind(sockfd, (sockaddr *) &addr, sizeof(addr)) == -1) {
		perror("Error binding stream socket");
		return -1;
	}

    // (3b)Detect which port was chosen.
	printf("Server listening on port %d...\n", port);

	// (4) Begin listening for incoming connections.
	listen(sockfd, queue_size);

    // (5) Serve incoming connections one by one forever.
	while (true) {
		int connectionfd = accept(sockfd, 0, 0);
		if (connectionfd == -1) {
			perror("Error accepting connection");
			return -1;
		}

		printf("New connection %d\n", connectionfd);

		// (5.1) Receive message from client.

		char msg[MAX_MESSAGE_SIZE + 1];
		memset(msg, 0, sizeof(msg));

		// Call recv() enough times to consume all the data the client sends.
		size_t recvd = 0;
		ssize_t rval;
		do {
			// Receive as many additional bytes as we can in one call to recv()
			// (while not exceeding MAX_MESSAGE_SIZE bytes in total).
			// Note that without and Flags, the number of bytes you recv() can be different than specified.
			rval = recv(connectionfd, msg + recvd, MAX_MESSAGE_SIZE - recvd, 0);
			if (rval == -1) {
				perror("Error reading stream message");
				exit(1);
			}
			recvd += rval;
		} while (rval == 0);  // recv() returns 0 when client closes, in project, you should a different logic.

        
		// (5.2) Print out the message
		printf("Client %d says '%s'\n", connectionfd, msg);

        sendFile(connectionfd, msg);

		// (5.3) Close connection
		close(connectionfd);

	}
}


int run_client(const char *hostname, int port, const char *message, int client_port)
{
    if (strlen(message) > MAX_MESSAGE_SIZE) {
		perror("Error: Message exceeds maximum length\n");
		return -1;
	}

    // (1) Create a socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// (2) Create a sockaddr_in to specify remote host and port
	struct sockaddr_in server_addr; // specify server's address
	server_addr.sin_family = AF_INET;

    // Step (2): specify socket address (hostname).
	// The socket will be a client, so call this unix helper function to convert a hostname string to 
	// a useable `hostent` struct. hostname here may be 10.0.0.1
	struct hostent *host = gethostbyname(hostname);
	if (host == nullptr) {
		fprintf(stderr, "%s: unknown host\n", hostname);
		return -1;
	}
	memcpy(&(server_addr.sin_addr), host->h_addr, host->h_length);

	// if we also we also want to bind client's socket to a port number...
	struct sockaddr_in my_addr;
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = INADDR_ANY; //INADDRY_ANY == 0.0.0.0
	my_addr.sin_port = htons(client_port);
	bind(sockfd, (struct sockaddr *) & my_addr, sizeof(my_addr));


	// Step (3): Set the port value.
	// Use htons to convert from local byte order to network byte order.
	server_addr.sin_port = htons(port);


	// (3) Connect to remote server
	if (connect(sockfd, (sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
		perror("Error connecting stream socket");
		return -1;
	}

    printf("Sending message %s to %s:%d\n", message, hostname, port);

	// (4) Send message to remote server and receive file
	if (send(sockfd, message, strlen(message), 0) == -1) {
		perror("Error sending on stream socket");
		return -1;
	}


    recvFile(sockfd, message);

	// (5) Close connection
	close(sockfd);

	return 0;
}


void sendFile(int sockfd, const char *filename) 
{ 
    printf("Sending file...\n");
 
    FILE *fp;
    fp=fopen(filename,"rb");  
    if(fp == NULL){
        printf("Error opening file. \n");
        return;
    }

    // Get length
    fseek(fp, 0L, SEEK_END);
    long length = ftell(fp);

    if (send(sockfd, &length, sizeof(long), 0) == -1) {
		perror("Error sending on stream socket");
		return;
	}

    rewind(fp);
    
	char* buff = (char*)malloc(MAX_MESSAGE_SIZE);    // for read operation from file and used to sent operation 

    while(!feof(fp)){
        int n = fread(buff, 1, MAX_MESSAGE_SIZE, fp);
        if (n > 0) { 
            if((n = send(sockfd, buff, MAX_MESSAGE_SIZE, 0)) < 0) 
            {
                printf("Error sending file");
            }
        }
    }
	//printf("%s\n", buff);
	

	/*
	char *buffer = (char*)malloc(sizeof(char)*length);
	fread(buffer, sizeof(char), length, fp);

	if(length < MAX_MESSAGE_SIZE)
        send(sockfd, buffer, length, 0);
        //assuming bytes sent == filesize
    else
    {
        int bytes_sent = 0;
        while(bytes_sent < length)
        {
            bytes_sent += send(sockfd, buffer+bytes_sent, MAX_MESSAGE_SIZE-1 , 0);
        }
    }*/
    
	free(buff);
    fclose(fp);       // close the file 
 
    printf("File Sent successfully.\n");
 
}


void recvFile(int sockfd, const char* filename) 
{ 
    printf("Receiving file...\n");
 
    FILE *fp;
    fp=fopen(filename,"wb"); // stores the file content in recieved.txt in the program directory
 
    if(fp == NULL){
        printf("Error opening file.\n");
        return;
    }

    // Get file length
    long length;

    ssize_t rval = recv(sockfd, &length, sizeof(long), 0);
	if (rval == -1) {
		perror("Error reading stream message");
		exit(1);
	}
    printf("Length of the file: %ld\n", length);

    
    size_t recvd = 0;
    char *buff = (char*)malloc(sizeof(char)*length);  // to store message from client
    while(1)
    {
        rval = recv(sockfd, buff + recvd, sizeof(char)*length - recvd, 0);
		//printf("%s\n", buff);
		recvd += rval;
        if (rval == 0)
            break;
    }
	fwrite(buff, 1, sizeof(char)*length, fp);

    free(buff);

    fclose(fp);
    printf("File received successfully !\n");
}

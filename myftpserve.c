#include "myftp.h"

#define BACKLOG 4
#define DATAREQ 1
#define CHANGEDIR 2
#define LISTDIR 3
#define GETFILE 4
#define PUTFILE 5
#define QUIT 6

void errnoExit(void){
    printf("Error: %s\n", strerror(errno) );
    fflush(stdout);
    fflush(stderr);
    exit(-errno);
}
void gai_errExit(int err){
    printf("Error: %s\n", gai_strerror(err) );
    fflush(stdout);
    fflush(stderr);
    exit(-err);
}
void errExit(char* msg){
	fprintf(stderr, "Error: %s.\n", msg);
	exit(-1);
}
void errMsg(char* msg){
	fprintf(stderr, "Error: %s.\n", msg);
}
void debugMsg(int child, char* msg){
	if (child) fprintf(stdout, "Child_%d_Debugger: %s.\n",child, msg);
	else fprintf(stdout, "Server_Debugger: %s.\n", msg);
}
int readControl(int fd, char** heapBuf){
	int toggle = 2;
	char buf[BUFFSIZE];
	int readNum = 0;
	for (int i=0;(readNum = read(fd, &buf[i], 1)) > 0;i++){
		if (buf[i] == ' ' && toggle){
			i--;
			continue;
		}
		if (toggle) toggle--;
		if (buf[i] == '\n'){
			buf[i] = '\0';
			if (buf[0] == 'D'){
				strcpy(*heapBuf,buf+1);
				return DATAREQ;
			}
			else if (buf[0] == 'C'){
				strcpy(*heapBuf,buf+1);
				return CHANGEDIR;
			}
			else if (buf[0] == 'L'){
				strcpy(*heapBuf,buf+1);
				return LISTDIR;
			}
			else if (buf[0] == 'G'){
				strcpy(*heapBuf,buf+1);
				return GETFILE;
			}
			else if (buf[0] == 'P'){
				strcpy(*heapBuf,buf+1);
				return PUTFILE;
			}
			else if (buf[0] == 'Q'){
				strcpy(*heapBuf,buf+1);
				return QUIT;
			}
			break;
		}
	}
	if (!readNum) errExit("Socket broken, child exiting");
	if (readNum == -1) errMsg(strerror(errno));
	return -1;
}

int socketMsg(int fd, char* type, char* msg){
	if (write(fd, type, 1) < 0) {
		fprintf(stderr, "Error: Pipe message failed to write\n");
		return -1;
	}
	if (msg){
		if (write(fd, msg, strlen(msg)) < 0) {
			fprintf(stderr, "Error: Pipe message failed to write\n");
			return -1;
		}
	}
	if (write(fd, "\n", 1) < 0) {
		fprintf(stderr, "Error: Pipe message failed to write\n");
		return -1;
	}
	return 0;
}

int changeDir(int fd, char* buf){
	int errSave = 0;
	struct stat fileStuff, *statP = &fileStuff;
	if (lstat(buf, statP) == 0){
		if (S_ISDIR(statP->st_mode)){
			if ((statP->st_mode & S_IRUSR) && (statP->st_mode & S_IXUSR)){
				errSave	= chdir(buf);
				if (errSave) {
					socketMsg(fd, "E", strerror(errno));
					return -1;
				} 
				return 0;
			}
		} else {
			socketMsg(fd, "E", "Path specified is not a directory");
		}
	} else {
		socketMsg(fd, "E", strerror(errno));
	}
	return -1;
}

int main(int argc, char const *argv[]){
	int debugSwitch = 0;
    int errSave = 0;
    errno = 0;

    if (argc < 1) errExit("Not enough arguments provided");
    if (argc > 1){
    	if (strcmp(argv[1],"-d")) errExit("Incorrect 1st argument");
    	debugSwitch = 1;
	    argv++;
	}
    if(debugSwitch) debugMsg(0, "Debug switch turned on");

    int listenFD = 0;
    int controlFD = 0;
    int readNum = 0;
    struct sockaddr_in server;
    struct sockaddr_in client;
    socklen_t clientLen = sizeof(client);
    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));

    server.sin_family = AF_INET;
    server.sin_port = htons(PORTNUM);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    listenFD = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFD == -1) errnoExit();
    if(debugSwitch) printf("Server_Debugger: Socket on FD %d.\n",
    						 listenFD);

    //Bens black magic for making sockets available
    errSave = setsockopt(listenFD, SOL_SOCKET, SO_REUSEADDR, &(int){1},
    						sizeof(int));
    if (errSave == -1) errnoExit();

    errSave = bind(listenFD, (const struct sockaddr *)&server, sizeof(server));
    if ( errSave == -1 ) errnoExit();
    if(debugSwitch)printf("Server_Debugger: Port %d bounded.\n", PORTNUM);

    errSave = listen( listenFD, BACKLOG);
    if (errSave == -1) errnoExit();
    if(debugSwitch)printf("Server_Debugger: Listening on port "
    						PORTNUMSTR" with a backlog of %d.\n", BACKLOG);

    int childPID = 0;
	printf("Server: Running.\n");
    while(1){
        if(debugSwitch) debugMsg(0, "Ready to accept connections");
        controlFD = accept(listenFD, (struct sockaddr *)&client, &clientLen);
        if ( controlFD == -1 ) errnoExit();
        if ((childPID = fork())) {
        	if(debugSwitch) printf("Server_Debugger: Child %d handling "
        		"connection.\n",childPID);
            close(controlFD);
            while ( waitpid( 0, NULL, WNOHANG ) > 0 ){
            	if(debugSwitch) debugMsg(0, "A child has been cleaned");
            }
            continue;
        }
        char *buf = calloc(BUFFSIZE,1);
        int myID = getpid();
        int commandCode = 0;
        if(debugSwitch)printf("Child_%d_Debugger: Started.\n",myID);
        char hostName[NI_MAXHOST];
        errSave = getnameinfo((struct sockaddr *)&client,
            sizeof(client), hostName,sizeof(hostName),NULL,0,
            NI_NUMERICSERV);
        if (errSave != 0) {
            gai_errExit(errSave);
        }
        printf("Child %d: Connection accepted from host" 
        	" %s.\n",myID,hostName);
        while(1){
        	if(debugSwitch) debugMsg(myID, "Ready for next command");
	        commandCode = readControl(controlFD,&buf);
	        if (commandCode == -1){
	        	free(buf);
	        	socketMsg(controlFD,"E","Could not read command");
	        	continue;
	        }
	        if (commandCode == DATAREQ){
	        	int dataFD = 0;
			    int readNumData = 0;
			    char dataBuf[PATH_MAX+2];
			    struct sockaddr_in serverData;
			    struct sockaddr_in clientData;
			    memset( &serverData, 0, sizeof(serverData));
			    memset( &clientData, 0, sizeof(clientData));

			    serverData.sin_family = AF_INET;
			    serverData.sin_port = htons(0);
			    serverData.sin_addr.s_addr = htonl(INADDR_ANY);

			    dataFD = socket(AF_INET, SOCK_STREAM, 0);
			    if (dataFD == -1) {
		        	socketMsg(controlFD,"E","Could not establish data socket");
		        	continue;
			    }
			    if(debugSwitch) printf("Child_%d_Debugger: Data socket on FD "
			    						"%d.\n", myID, dataFD);

			    //Bens black magic for making sockets available
			    errSave = setsockopt(dataFD, SOL_SOCKET, SO_REUSEADDR, &(int){1},
			    						sizeof(int));
			    if (errSave == -1) {
		        	socketMsg(controlFD,"E","Could not establish data socket");
		        	continue;
			    }

			    errSave = bind(dataFD, (const struct sockaddr *)&serverData,
			    				 sizeof(serverData));
			    if ( errSave == -1 ) {
		        	socketMsg(controlFD,"E","Could not establish data socket");
		        	continue;
			    }
			    socklen_t servSize = sizeof(serverData);
			    errSave = getsockname(dataFD, (struct sockaddr *)&serverData,
			    						 &servSize);
			    if ( errSave == -1 ) {
		        	socketMsg(controlFD,"E","Could not establish data socket");
		        	continue;
			    }
			    if(debugSwitch) printf("Child_%d_Debugger: Retrieving ephemeral"
			    						" port.\n", myID);
			    int tempPort = ntohs(serverData.sin_port);
			    char tempChar[SOCK_CHAR_MAX+1];
			    sprintf(tempChar,"%d",tempPort);
			    tempChar[SOCK_CHAR_MAX] = '\0';
			    if(debugSwitch) printf("Child_%d_Debugger: Port %s bounded.\n",
			    						 myID, tempChar);
			    errSave = listen( dataFD, 1);
			    if (errSave == -1) {
		        	socketMsg(controlFD,"E","Could not establish data socket");
		        	continue;
			    }
			    if(debugSwitch)printf("Child_%d_Debugger: Listening on port"
			    						" %d.\n", myID, tempPort);
			    socketMsg(controlFD, "A", tempChar);
				if(debugSwitch)printf("Child_%d_Debugger: Sending acknowledge.\n",myID);

				int oldFD = dataFD;
				dataFD = accept(dataFD, NULL, NULL);
				if ( dataFD == -1 ){
		        	socketMsg(controlFD,"E","Could not establish data socket");
		        	continue;
				}
				close(oldFD);
				if(debugSwitch)printf("Child_%d_Debugger: accepted data "
										"connection.\n",myID);
				commandCode = readControl(controlFD,&buf);
		        if (commandCode == -1){
		        	socketMsg(controlFD,"E","Could not read command");
		        	continue;
		        }
		        int cPid = 0;
		        if (commandCode == LISTDIR){
		        	if(debugSwitch)printf("Child_%d_Debugger: Recieved ls.\n",
		        		 					myID);
					socketMsg(controlFD, "A", NULL);
					if(debugSwitch) printf("Child_%d_Debugger: Sending "
											"acknowledge.\n",myID);
		        	if(debugSwitch) printf("Child_%d_Debugger: Running ls -al."
		        							"\n",myID);
					if ((cPid = fork())){
						waitpid(cPid, NULL, 0);
						close(dataFD);
						close(dataFD);
					} else {
						printf("Child %d: Listing current directory as requested"
								" from %s.\n", myID, hostName);
						char* lsArg[] = {"ls", "-al", NULL};
						close(1);
						dup(dataFD);
						errSave = execvp("ls",lsArg);
						socketMsg(controlFD,"E","Exec ls failed");
		        		exit(-1);
					}
		        }
		        else if (commandCode == GETFILE){
		        	if(debugSwitch)printf("Child_%d_Debugger: Recieved get.\n",
		        		 					myID);
					socketMsg(controlFD, "A", NULL);
					if(debugSwitch)printf("Child_%d_Debugger: Sending "
											"acknowledge.\n",myID);
					if ((cPid = fork())){
			    		if(debugSwitch)printf("Child_%d_Debugger: Forked child "
			    								"%d.\n", myID, cPid);
				    	waitpid(cPid, NULL, 0);
				    	close(dataFD);
				    	continue;
				    } else {
				    	char sockBuf[SOCK_BYTE_MAX];
				    	int giveFD = open(buf, O_RDONLY, 0);
				    	while ((readNum = read(giveFD, sockBuf,
				    							 SOCK_BYTE_MAX)) > 0){
				    		if (write(dataFD, sockBuf, readNum) < 0){
				    			socketMsg(controlFD,"E","File transfer failed");
								break;
				    		}
				    	}
				    	printf("Child %d: Sending file %s to %s.\n", myID, buf,
				    			 hostName);
				    	close(giveFD);
				    	if (readNum == -1) socketMsg(controlFD,"E",
				    								strerror(errno));
				    	exit(0);
				    }
		        }
		        else if (commandCode == PUTFILE){
		        	if(debugSwitch)printf("Child_%d_Debugger: Recieved put.\n",
		        							myID);
					socketMsg(controlFD, "A", NULL);
					if(debugSwitch)printf("Child_%d_Debugger: Sending "
											"acknowledge.\n",myID);

					char sockBuf[SOCK_BYTE_MAX];
			    	int pathFile = 0;
			    	for (int i=0;buf[i] != '\0';i++){
			    		if (buf[i] == '/') pathFile = i+1;
			    	}
			    	int putFD = open(buf+pathFile,
			    					 O_WRONLY|O_CREAT|O_TRUNC, 0644);
			    	if (!putFD){
			    		close(putFD);
			    		errnoExit();
			    	}
			    	printf("Child %d: Recieving file %s"
								" from %s.\n", myID, buf+pathFile, hostName);
			    	if(debugSwitch) printf("Child_%d_Debugger: File opened.\n",
			    		 					getpid());
			    	printf("Child %d: Loading", getpid());
			    	int bigFile = LOADTIME;
			    	while ((readNum = read(dataFD, sockBuf,
			    							 SOCK_BYTE_MAX)) > 0){
			    		if(write(putFD, sockBuf, readNum) < 0){
			    			close(putFD);
			    			errnoExit();
			    		}
			    		if (!bigFile) {
			    			bigFile = LOADTIME;
			    			printf(".");
			    		}
			    		bigFile--;
			    		fflush(stdout);
			    	}
			    	printf("\n");
			    	printf("Child %d: File recieved.\n", myID);
			    	fflush(stdout);
			    	continue;
		        }
	        }
	        else if (commandCode == CHANGEDIR){
	        	if(debugSwitch)printf("Child_%d_Debugger: Recieved cd.\n",myID);
				socketMsg(controlFD, "A", NULL);
				if(debugSwitch)printf("Child_%d_Debugger: Sending acknowledge.\n",
										myID);
	        	if (changeDir(controlFD, buf)) continue;
				printf("Child %d: Succesful cd to %s requested by %s.\n", myID,
						 buf, hostName);
				continue;
	        }
	        else if (commandCode == QUIT){
	        	if(debugSwitch) printf("Child_%d_Debugger: Recieved quit.\n",
	        							myID);
				socketMsg(controlFD, "A", NULL);
				if(debugSwitch) printf("Child_%d_Debugger: Sending "
										"acknowledge.\n",myID);
				if(debugSwitch) printf("Child_%d_Debugger: Exiting.\n",myID);
				close(controlFD);
				printf("Child %d: Disconnected from %s.\n", myID, hostName);
				exit(0);
	        }
	        else {
				socketMsg(controlFD,"E", "Recieved command that requires data"
							" connection before one was established");
				continue;
			}
	    }
    }
}

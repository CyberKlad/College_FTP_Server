#include "myftp.h"

#define ADDRARG 0

void errnoExit(void){
    fprintf(stderr, "Error: %s.\n", strerror(errno));
    exit(-1);
}

void gai_errExit(int err){
    fprintf(stderr, "Error: %s.\n", gai_strerror(err));
    exit(-1);
}

void errExit(char* msg){
	fprintf(stderr, "Error: %s.\n", msg);
	exit(-1);
}

void errMsg(char* msg){
	fprintf(stderr, "Error: %s.\n", msg);
	fflush(stderr);
}

void debugMsg(char* msg){
	fprintf(stdout, "Client_Debugger: %s.\n", msg);
}

int changeDir(char* userPath){
	int errSave = 0;
	struct stat fileStuff, *statP = &fileStuff;
	if (lstat(userPath, statP) == 0){
		if (S_ISDIR(statP->st_mode)){
			if ((statP->st_mode & S_IRUSR) && (statP->st_mode & S_IXUSR)){
				errSave	= chdir(userPath);
				if (errSave) {
					errMsg(strerror(errno));
					return -1;
				}
				return 0;
			}
		} else {
			errMsg("Path specified is not a directory");
		}
	} else {
		errMsg(strerror(errno));
	}
	return -1;
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
			if (buf[0] == 'E'){
				errMsg(buf+1);
				strcpy(*heapBuf,buf+1);
				return -1;
			}
			if (buf[0] == 'A'){
				strcpy(*heapBuf,buf+1);
				return 0;
			}
			break;
		}
	}
	if (!readNum) errMsg("Socket broken");
	if (readNum == -1) errMsg(strerror(errno));
	return -1;
}

int socketMsg(int fd, char* type, char* msg){
	if (write(fd, type, 1) < 0) {
		errExit("Socket broken");
		return -1;
	}
	if (msg){
		if (write(fd, msg, strlen(msg)) < 0) {
			errExit("Socket broken");
			return -1;
		}
	}
	if (write(fd, "\n", 1) < 0) {
		errExit("Socket broken");
		return -1;
	}
	return 0;
}

int main(int argc, char const *argv[]){
	signal(SIGPIPE,SIG_IGN);
	errno = 0;
	int errSave = 0;

	char *buf = calloc(BUFFSIZE,1);
	int readNum = 0;
	int controlFD = 0;
	int childPID = 0;
	int debugSwitch = 0;

    if (argc > 3)errExit("Too many arguments provided");
    if (argc < 2)errExit("Not enough arguments provided");
    if (!(strcmp(argv[1], "-d"))){
    	debugSwitch = 1;
    	if (debugSwitch) debugMsg("Debug switch turned on");
    	argv++;
    	if (argc < 3) errExit("Not enough arguments provided");
    }
    argv++;

    struct addrinfo clue, *actual;
    memset(&clue, 0, sizeof(clue));

    clue.ai_socktype = SOCK_STREAM;
    clue.ai_family = AF_INET;

    errSave = getaddrinfo(argv[ADDRARG], PORTNUMSTR, &clue, &actual);
    if (errSave) gai_errExit(errSave);
    if (debugSwitch) printf("Client_Debugger: Resolved %s on port %s.\n",
    						argv[ADDRARG], PORTNUMSTR);

    controlFD = socket(actual->ai_family, actual->ai_socktype, 0);
    if (controlFD == -1) errnoExit();
    if (debugSwitch) printf("Client_Debugger: Socket connected on FD %d.\n",
    						controlFD);

    errSave = connect(controlFD, actual->ai_addr, actual->ai_addrlen);
    if (errSave == -1){
    	freeaddrinfo(actual);
    	errnoExit();
    }
    printf("Client: Connection to %s established.\n",argv[ADDRARG]);

    char userCmd[5];
    memset(&userCmd, 0, sizeof(userCmd));
    char userPath[PATH_MAX+1];
    memset(&userPath, 0, sizeof(userPath));
    while(1){
    	printf("FTP_Client> ");
		fflush(stdout);
		errSave = read(0, buf, BUFFSIZE);
		if (errSave == -1){
			errMsg("User input read failed");
			continue;
		}
		sscanf(buf,"%s %s",userCmd,userPath);
		for (int i=0;i<5;i++){
			if (userCmd[i] == 0){
				userCmd[i] = '\0';
				break;
			}
		}
		for (int i=0;i<PATH_MAX+1;i++){
			if (userPath[i] == 0){
				userPath[i] = '\0';
				break;
			}
		}
		if (debugSwitch) debugMsg("Input has been read");

		if (!(strcmp(userCmd, "exit"))){
			if (socketMsg(controlFD, "Q", NULL)) continue;
			if (debugSwitch) debugMsg("Exit command sent to server");
			if (readControl(controlFD, &buf)) continue;
			if (debugSwitch) debugMsg("Server acknowledged");
			printf("Client: Exiting.\n");
			break;
		}

		else if (!(strcmp(userCmd,"cd"))){
			if (changeDir(userPath)) continue;
			printf("Client: Succesful cd to %s.\n",userPath);
			continue;
		}

		else if (!(strcmp(userCmd,"rcd"))){
			if (socketMsg(controlFD, "C", userPath)) continue;
			if (debugSwitch) printf("Client: Requesting server cd to %s.\n",
									userPath);
			if (readControl(controlFD, &buf)) continue;
			if (debugSwitch) debugMsg("Server acknowledged");
			printf("Client: Server cd to %s.\n",userPath);
			continue;
		}

		else if (!(strcmp(userCmd,"ls"))){
			if (debugSwitch) printf("Client: Running ls -al | more -n 20 "
									"locally.\n");
			if ((childPID = fork())){
				waitpid(childPID, NULL, 0);
				continue;
			} else {
				int pipeFD[2];
    			pipe(pipeFD);
				char* moreArg[] = {"more","-n", "20", NULL};
				char* lsArg[] = {"ls", "-al", NULL};
				if (!(fork())){
					close(1);
			        close(pipeFD[0]);
			        dup(pipeFD[1]);
			        close(pipeFD[1]);
					execvp("ls",lsArg);
					errnoExit();
				} else {
					close(0);
			        close(pipeFD[1]);
			        dup(pipeFD[0]);
			        close(pipeFD[0]);
					execvp("more",moreArg);
					errnoExit();
				}
			}
		}

		else if (!(strcmp(userCmd,"rls")) || !(strcmp(userCmd,"put")) ||
					!(strcmp(userCmd,"show")) || !(strcmp(userCmd,"get"))){
			if (socketMsg(controlFD, "D", NULL)) continue;
			if (debugSwitch) debugMsg("Requesting Data connection");
			if (readControl(controlFD, &buf)) continue;
			if (debugSwitch) printf("Client_Debugger: Recieved port %s.\n",
									buf);

			int dataFD = 0;
		    struct addrinfo data, *actualData;
		    memset(&data, 0, sizeof(data));
		    data.ai_socktype = SOCK_STREAM;
		    data.ai_family = AF_INET;

		    errSave = getaddrinfo(argv[ADDRARG], buf, &data, &actualData);
		    if (errSave){
		    	errMsg((char*)gai_strerror(errSave));
		    	continue;
		    }
		    if(debugSwitch) printf("Client_Debugger: Resolved %s on port %s.\n",
		    						argv[ADDRARG], buf);

		    dataFD = socket( actualData->ai_family, actualData->ai_socktype,
		    				 0 );
		    if (dataFD == -1) {
		    	errMsg(strerror(errno));
				freeaddrinfo(actualData);
		    	continue;
		    }
		    if(debugSwitch) printf("Client_Debugger: Socket connected on FD "
		    						"%d.\n", dataFD);

		    errSave = connect( dataFD, actualData->ai_addr,
		    					 actualData->ai_addrlen );
		    if (errSave == -1) {
		    	errMsg(strerror(errno));
		    	close (dataFD);
				freeaddrinfo(actualData);
		    	continue;
		    }
		    if(debugSwitch) printf("Client_Debugger: Connected to %s.\n",
		    						argv[ADDRARG]);

		    if (!(strcmp(userCmd,"rls"))){
		    	if (socketMsg(controlFD, "L", NULL)) {
		    		close (dataFD);
					freeaddrinfo(actualData);
		    		continue;
		    	}
		    	if (debugSwitch) debugMsg("Sending rls to server");
		    	if (readControl(controlFD, &buf)) {
		    		close (dataFD);
					freeaddrinfo(actualData);
		    		continue;
		    	}
				if (debugSwitch) debugMsg("Server acknowledged");
				if ((childPID = fork())){
					if(debugSwitch)printf("Client_Debugger: Forked child %d.\n",
											childPID);
			    	waitpid(childPID, NULL, 0);
			    	close(dataFD);
			    	freeaddrinfo(actualData);
			    	continue;
			    } else {
			    	char* moreArg[] = {"more","-n", "20", NULL};
			    	close(0);
			        dup(dataFD);
					execvp("more",moreArg);
					errnoExit();
			    }
		    }

		    else if (!(strcmp(userCmd,"put"))){
		    	int pathFile = 0;
		    	for (int i=0;userPath[i] != '\0';i++){
		    		if (userPath[i] == '/') pathFile = i+1;
		    	}
		    	if (socketMsg(controlFD, "P", userPath+pathFile)) {
		    		close (dataFD);
					freeaddrinfo(actualData);
		    		continue;
		    	}
		    	if (debugSwitch) debugMsg("Sending put to server");
		    	if (readControl(controlFD, &buf)) {
		    		close (dataFD);
					freeaddrinfo(actualData);
		    		continue;
		    	}
				if (debugSwitch) debugMsg("Server acknowledged");
		    	if ((childPID = fork())){
		    		if(debugSwitch)printf("Client_Debugger: Forked child %d.\n",
		    								childPID);
			    	waitpid(childPID, NULL, 0);
			    	close(dataFD);
			    	freeaddrinfo(actualData);
			    	continue;
			    } else {
			    	char sockBuf[SOCK_BYTE_MAX];
			    	int putFD = open(userPath, O_RDONLY, 0);
			    	while ((readNum = read(putFD, sockBuf, SOCK_BYTE_MAX)) > 0){
			    		if (write(dataFD, sockBuf, readNum) != readNum){
			    			printf("Error: %s\n", strerror(errno) );
							break;
			    		}
			    	}
			    	close(putFD);
			    	printf("Child %d: Server put %s.\n",getpid(),
			    			 userPath+pathFile);
			    	if (readNum == -1) errnoExit();
			    	exit(0);
			    }
		    }

		    else if (!(strcmp(userCmd,"show")) || !(strcmp(userCmd,"get"))){
		    	if (socketMsg(controlFD, "G", userPath)) {
		    		close (dataFD);
					freeaddrinfo(actualData);
		    		continue;
		    	}
		    	if (debugSwitch) debugMsg("Sending get to server");
		    	if (readControl(controlFD, &buf)) {
		    		close (dataFD);
					freeaddrinfo(actualData);
		    		continue;
		    	}
				if (debugSwitch) debugMsg("Server acknowledged");

		    	if (!(strcmp(userCmd,"get"))){
		    		if ((childPID = fork())){
		    			if(debugSwitch)printf("Client_Debugger: Forked child "
		    									"%d.\n",childPID);
				    	waitpid(childPID, NULL, 0);
				    	close(dataFD);
				    	freeaddrinfo(actualData);
				    	continue;
				    } else {
				    	char sockBuf[SOCK_BYTE_MAX];
				    	int pathFile = 0;
				    	for (int i=0;userPath[i] != '\0';i++){
				    		if (userPath[i] == '/') pathFile = i+1;
				    	}
				    	int getFD = open(userPath+pathFile,
				    					 O_WRONLY|O_CREAT|O_TRUNC, 0644);
				    	if (!getFD){
				    		close(getFD);
				    		errnoExit();
				    	}
				    	printf("Child %d: File opened.\nLoading",getpid());
				    	int bigFile = LOADTIME;
				    	while ((readNum = read(dataFD, sockBuf,
				    							 SOCK_BYTE_MAX)) > 0){
				    		if(write(getFD, sockBuf, readNum) < 0){
				    			close(getFD);
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
				    	printf("Child %d: File %s transfered.\n", getpid(),
				    			 userPath+pathFile);
				    	fflush(stdout);
				    	exit(0);
				    }
		    	}

		    	if (!(strcmp(userCmd,"show"))){
		    		if ((childPID = fork())){
		    			if(debugSwitch)printf("Client_Debugger: Forked child "
		    									"%d.\n",childPID);
				    	waitpid(childPID, NULL, 0);
				    	close(dataFD);
				    	freeaddrinfo(actualData);
				    	continue;
				    } else {
				    	char* moreArg[] = {"more","-n", "20", NULL};
				    	close(0);
				        dup(dataFD);
						execvp("more",moreArg);
						errnoExit();
				    }
		    	}
		    }
		}

		else {
			printf("Error: Incorrect command issued.\n"
					"Info: The correct commands are:\n\nexit\ncd <pathname>\n"
					"rcd <pathname>\nls\nrls\nget <pathname>\nshow <pathname>\n"
					"put <pathname>\n\n");
		}
    }
    close(controlFD);
	freeaddrinfo(actual);
	free(buf);
	exit(0);
}

#include <stdio.h>
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>
#define MAXLINE 100

pthread_mutex_t mutex;

int menu(){
	int op;
	char input[MAXLINE];
	do{
		printf("\n-------------MENU-------------\n");
		printf("1. Play.\n");
		printf("2. How to play.\n");
		printf("3. High score.\n");
		printf("4. Credits.\n");
		printf("5. Exit.\n");
		printf("Enter your choice: ");
		scanf(" %[^\n]", input);
	    if (strlen(input) != 1 || !isdigit(input[0])) break;
	    op = atoi(input);
	}while(op > 5 || op < 1);
	return op;
}

int menuplay(){
	char input[MAXLINE];
	int op;
	do{
		printf("\n************************************\n");
		printf("\tMenu play\n");
		printf("\t1. Change password.\n");
		printf("\t2. Choose mode.\n");
		printf("\t3. Log out.\n");
		printf("\t4. Exit.\n");
		scanf(" %[^\n]", input);
	    if (strlen(input) != 1 || !isdigit(input[0])) break;
	    op = atoi(input);
	}while(op > 4 || op < 1);
	return op;
}

void *recvmg(void *my_sock){
	int sockfd = *((int *)my_sock);
    int len;
    char data[MAXLINE], str[MAXLINE];
    int n, op_play;
	while(1) {
	    n =  recv(sockfd,data,MAXLINE,0 );
	    if(n == 0 ){
	        perror("The server terminated prematurely");
	        return 0;
	    }
	    data[n] = '\0';
	    printf("[%s]\n", data);
     }
}

int main() {
	int recvBytes, sendBytes;
	char sendBuff[MAXLINE] = {0}, recvBuff[MAXLINE]; 
	int sockfd = 0, valread;
    pthread_t recvt;
    struct sockaddr_in servaddr, cliaddr; 
    char ser_address[MAXLINE] = {0};
    // menu
	int op, op_play;
	char str[MAXLINE] = {0};
	do{
		op = menu();
		switch(op){
			case 1:
			// ket noi voi server
				if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){ 
			        printf("\n Socket creation error \n"); 
			        return -1; 
			    } 
			    printf("Enter Server Address: ");
			    scanf(" %[^\n]", ser_address);

			    memset(&servaddr, 0, sizeof(servaddr));
			    servaddr.sin_family = AF_INET;
			    servaddr.sin_addr.s_addr = inet_addr(ser_address);
			    inet_aton(ser_address, &servaddr.sin_addr);
			    servaddr.sin_port = htons(5000);

			    printf("\n************************************\nConnecting....\n\n");
			    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){ 
			        printf("\nConnection Failed \n"); 
			        return -1; 
			    }
			    else {
			    	printf("Connected!\n************************************\n\n");
			    	char c;
			    	scanf("%c", &c);
			    	pthread_mutex_lock(&mutex);
				    if (pthread_create(&recvt, NULL,(void *)recvmg, &sockfd) < 0) {
				    	printf("Error creating thread\n");
				    	exit(1);
				    }
				    while(fgets( sendBuff, MAXLINE, stdin) != NULL){
				        char *tmp = strstr(sendBuff, "\n");
				        if(tmp != NULL) *tmp = '\0';
				        int check = 0;
				        for(int i = 0; i < strlen(sendBuff); i ++){
				            if(sendBuff[i] != ' ' && sendBuff[i] != '\0'){
				                check = 1;
				                break;
				            }
				        }
				        if(check == 0) break;
				        send(sockfd , sendBuff , strlen(sendBuff) , 0 );
				        recvBytes = recv(sockfd, recvBuff, MAXLINE, 0);
				    	if (recvBytes == 0) {
				    	    perror("The server terminated prematurely");
				    	    exit(4);
				    	    return 0;
				    	} else {
				    	    recvBuff[recvBytes] = '\0';
				    	    printf("%s\n", recvBuff);
				    	    if(strcmp(recvBuff, "OK") == 0){
				    	    	do{
									op_play = menuplay();
									switch(op_play){
							// change password
										case 1:
											printf("Enter new password: ");
											scanf(" %[^\n]", str);
											send(sockfd , str , strlen(str) , 0 );
											recvBytes = recv(sockfd, recvBuff, MAXLINE, 0);
									    	if (recvBytes == 0) {
									    	    perror("The server terminated prematurely");
									    	    exit(4);
									    	    return 0;
									    	}
								    	    recvBuff[recvBytes] = '\0';
								    	    printf("%s\n", recvBuff);
										break;
							// choose mode
										case 2:
										break;
							// log out
										case 3:
										break;
										default: return 0;
									}
								}while(op != 4);
							}
				    	}
				    }
				    pthread_mutex_unlock(&mutex);
				}
				close(sockfd);
			break;
			case 2:
			break;
			case 3:
			break;
			case 4:
			break;
			default: break;
		}
	}while (op != 5);
}
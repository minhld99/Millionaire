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

int menu(){
	int op;
	printf("***********MENU*************\n");
	printf("1. Play.\n");
	printf("2. How to play.\n");
	printf("3. High score.\n");
	printf("4. Credits.\n");
	printf("5. Exit.\n");
	printf("Enter your choice: ");
	do{
		scanf("%d", &op);
	}while(op > 5 || op < 1);
	return op;
}

int menuplay(){
	int op;
	printf("\tMenu play\n");
	printf("1. Change password.\n");
	printf("2. Choose mode.\n");
	printf("3. Log out.\n");
	printf("4. Exit.\n");
	do{
		scanf("%d", &op);
	}while(op > 4 || op < 1);
	return op;
}

int recvBytes, sendBytes;
char sendBuff[MAXLINE] = {0}, recvBuff[MAXLINE];
void *recvmg(void *my_sock){
    int sockfd = *((int *)my_sock);
    int len;
    recvBytes =  recv(sockfd,recvBuff,MAXLINE,0 );
    if(recvBytes == 0 ){
        perror("The server terminated prematurely");
        return 0;
    }
    recvBuff[recvBytes] = '\0';
    printf("Message from server: %s\n", recvBuff);
}

int main() { 
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

			    printf("Connecting....\n");
			    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){ 
			        printf("\nConnection Failed \n"); 
			        return -1; 
			    }
			    printf("Connected!\n");
			    pthread_create(&recvt, NULL,(void *)recvmg, &sockfd);

				        
				op_play = menuplay();
			// gui cho server
				sprintf(str,"%d", op_play);
				send(sockfd, str, strlen(str), 0);
				// fix
				switch(op_play){
					// change password
					case 1:
						printf("Enter new password: ");
						scanf(" %[^\n]", str);
				        // char *tmp = strstr(str, "\n");
				        // if(tmp != NULL) *tmp = '\0';
						send(sockfd , str , strlen(str) , 0 );
						break;
					// choose mode
					case 2:
					break;
					// log out
					case 3:
					break;
					default: return 0;
				}
				pthread_join(recvt,NULL);
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
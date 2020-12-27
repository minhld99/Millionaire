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
#include <time.h>
#include <poll.h>
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"
#define MAXLINE 1000

char data[MAXLINE];
int end_game_online = 0, help = 0;
pthread_mutex_t mutex;

int menu(){
	int op;
	char input[MAXLINE];
	do{
		printf("\n-------------MENU-------------\n");
		printf("1. Play.\n");
		printf("2. How to play.\n");
		printf("3. Credits.\n");
		printf("4. Exit.\n");
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
		printf("\t1. Login\n");
		printf("\t2. Register\n");
		printf("\t3. Change password.\n");
		printf("\t4. Choose mode online.\n");
		printf("\t5. Choose mode offline.\n");
		printf("\t6. Log out.\n");
		printf("\t7. Exit.\n");
		scanf(" %[^\n]", input);
	    if (strlen(input) != 1 || !isdigit(input[0])) break;
	    op = atoi(input);
	}while(op > 7 || op < 1);
	return op;
}

void *recvmg(void *my_sock){
	int sockfd = *((int *)my_sock);
    int len;
    int n;
    while(1){
	    n =  recv(sockfd,data,MAXLINE,0 );
	    if(n == 0 ){
	        perror("The server terminated prematurely");
	        return 0;
	    }
	    data[n] = '\0';
	    printf("%s\n", data);
	    if(strstr(data, "Sai! Đáp án đúng là") == NULL 
	    && strstr(data, "Chúc mừng bạn đã trả lời đúng 15 câu hỏi!") == NULL
	    && strstr(data, "Không đủ người chơi online") == NULL
	    && strstr(data, "Mode online kết thúc") == NULL
	    && strstr(data, "Cần đăng nhập trước khi chơi!") == NULL){
			continue;
	    } else {
	    	end_game_online = 1;
	    	return 0;
	    }
	}
}

int main() {
	int recvBytes, sendBytes;
	char sendBuff[MAXLINE] = {0}, recvBuff[MAXLINE]; 
	int sockfd = 0, valread;
    pthread_t recvt;
    struct sockaddr_in servaddr, cliaddr; 
    char ser_address[MAXLINE] = "222.252.105.252"; //222.252.105.252
    // menu
	int op, op_play;
	char str[MAXLINE] = {0}, *input;
	printf(
           "            _    _                                               \n"
           "/'\\_/`\\ _  (_)  (_)  _                       _                 \n"
           "|     |(_) | |  | | (_)   _     ___     _ _ (_) _ __   __        \n"
           "| (_) || | | |  | | | | /'_`\\ /' _ `\\ /'_` )| |( '__)/'__`\\   \n"
           "| | | || | | |  | | | |( (_) )| ( ) |( (_| || || |  (  ___/      \n"
           "(_) (_)(_)(___)(___)(_)`\\___/'(_) (_)`\\__,_)(_)(_)  `\\____) \n\n");
	do{
		op = menu();
		switch(op){
			case 1:
			// ket noi voi server
				if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){ 
			        printf("\n Socket creation error \n"); 
			        return -1; 
			    }
			    printf("Server Address: %s", ser_address);
			    //scanf(" %[^\n]", ser_address);

			    memset(&servaddr, 0, sizeof(servaddr));
			    servaddr.sin_family = AF_INET;
			    servaddr.sin_addr.s_addr = inet_addr(ser_address);
			    inet_aton(ser_address, &servaddr.sin_addr);
			    servaddr.sin_port = htons(6000);

			    printf("\n************************************\n\nConnecting....\n\n");
			    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){ 
			        printf("\nConnection Failed \n"); 
			        return -1; 
			    }
			    else {
			    	printf("Connected!\n");
			    	// char c;
			    	// scanf("%c", &c);
			    	// pthread_mutex_lock(&mutex);
				    // if (pthread_create(&recvt, NULL,(void *)recvmg, &sockfd) < 0) {
				    // 	printf("Error creating thread\n");
				    // 	exit(1);
				    // }
				    do {
						op_play = menuplay();
						sprintf(str,"%d", op_play);
						send(sockfd, str,strlen(str), 0);
						//gui option sang server
						switch(op_play){
							// login
							case 1:
								recvBytes = recv(sockfd, recvBuff, MAXLINE, 0);
								if (recvBytes == 0) {
									perror("The server terminated prematurely");
									exit(4);
									return 0;
								}
								recvBuff[recvBytes] = '\0';
								printf("%s", recvBuff);
								if(strstr(recvBuff, "Bạn đang đăng nhập bằng tài khoản") != NULL) break;
								char ch;
								scanf("%c", &ch);
								while(fgets( sendBuff, MAXLINE, stdin) != NULL) {
									char *tmp = strstr(sendBuff, "\n");
									if(tmp != NULL) *tmp = '\0';
									int check = 0, count, answer;
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
										printf("%s", recvBuff);
										if(strstr(recvBuff, "OK") != NULL || strstr(recvBuff, "Tài khoản chưa sẵn sàng") != NULL || strstr(recvBuff, "Lỗi!") != NULL || strstr(recvBuff, "Tài khoản đang bị khóa.") != NULL || strstr(recvBuff, "Sai mật khẩu.") != NULL || strstr(recvBuff, "Tài khoản không tồn tại!") != NULL ) break;
									}
								}
								break;
							// register
							case 2:
								recvBytes = recv(sockfd, recvBuff, MAXLINE, 0);
								if (recvBytes == 0) {
									perror("The server terminated prematurely");
									exit(4);
									return 0;
								}
								recvBuff[recvBytes] = '\0';
								printf("%s", recvBuff);
								if(strstr(recvBuff, "Bạn đang đăng nhập bằng tài khoản") != NULL ) break;
								char c;
								scanf("%c", &c);
								while(fgets(sendBuff, MAXLINE, stdin) != NULL) {
									char *tmp = strstr(sendBuff, "\n");
									if(tmp != NULL) *tmp = '\0';
									int check = 0, count, answer;
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
										printf("%s", recvBuff);
										if(strstr(recvBuff, "Đăng ký thành công!") != NULL || strstr(recvBuff, "Tên tài khoản đã tồn tại!") != NULL) break;
									}
								}
								break;
							// change password
							case 3:
								recvBytes = recv(sockfd, recvBuff, MAXLINE, 0);
								if (recvBytes == 0) {
									perror("The server terminated prematurely");
									exit(4);
									return 0;
								}
								recvBuff[recvBytes] = '\0';
								printf("%s", recvBuff);
								if(strstr(recvBuff, "Bạn chưa đăng nhập,") != NULL) break;
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
								if(strstr(recvBuff, "Đổi mật khẩu thành công!") != NULL) break;
								break;
							// choose mode online
							case 4:

								pthread_create(&recvt, NULL,(void *)recvmg, &sockfd);
								// pthread_join(recvt, NULL);
								while(1){
									clock_t begin = clock();
									printf("");
									int answer = 0;
									char answer_str[MAXLINE];
									do {
										if (end_game_online == 1) break;
										struct pollfd mypoll = { STDIN_FILENO, POLLIN|POLLPRI };
										if(poll(&mypoll, 1, 2000)) scanf(" %[^\n]", str);
										else continue;
										str[0]=toupper(str[0]);
										if (strcmp(str, "A") == 0) {
											answer = 1;
										}
										else if (strcmp(str, "B") == 0) {
											answer = 2;
										}
										else if (strcmp(str, "C") == 0) {
											answer = 3;
										}
										else if (strcmp(str, "D") == 0) {
											answer = 4;
										}
										else if( strcmp(str,"H") == 0){
											answer = 5;
										}
										else if (strstr(str, "Sai! Đáp án đúng là") != NULL) break;
										if (answer == 0) sprintf(str,"%d", answer);
									} while (answer != 1 && answer != 2 && answer != 3 && answer != 4 && answer != 5 );
									if (end_game_online == 1) break;
									printf("  ");
									clock_t end = clock();
									double time_answer = (double)(end - begin) / CLOCKS_PER_SEC;
									sprintf(answer_str,"%d %f", answer, time_answer);
									printf("%s\n", str);
									send(sockfd , answer_str , strlen(answer_str) , 0 );
									printf("Time: %f\n", time_answer);
								}
								end_game_online = 0;
								break;
							// Choose mode offline
							case 5:
								while(1) {
									recvBytes = recv(sockfd, recvBuff, MAXLINE, 0);
									if (recvBytes == 0) {
										perror("The server terminated prematurely");
										exit(4);
										return 0;
									}
									recvBuff[recvBytes] = '\0';
									printf(CYN "%s" RESET, recvBuff);
									if(strstr(recvBuff, "Cần đăng nhập trước khi chơi!") == NULL && strstr(recvBuff, "Sai! Đáp án đúng là") == NULL && strstr(recvBuff, "Chúc mừng bạn đã trả lời đúng 15 câu hỏi!") == NULL) {
										do {
											scanf(" %[^\n]", str);
											str[0]=toupper(str[0]);
											int answer = 0;
											if (strcmp(str, "A") == 0) {
												answer = 1;
												sprintf(str,"%d", answer);
												send(sockfd , str , strlen(str) , 0 );
											}
											if (strcmp(str, "B") == 0) {
												answer = 2;
												sprintf(str,"%d", answer);
												send(sockfd , str , strlen(str) , 0 );
											}
											if (strcmp(str, "C") == 0) {
												answer = 3;
												sprintf(str,"%d", answer);
												send(sockfd , str , strlen(str) , 0 );
											}
											if (strcmp(str, "D") == 0) {
												answer = 4;
												sprintf(str,"%d", answer);
												send(sockfd , str , strlen(str) , 0 );
											}
											if (strcmp(str, "H") == 0) {
												answer = 5;
												sprintf(str,"%d", answer);
												send(sockfd , str , strlen(str) , 0 );
											}
											if (answer == 0) sprintf(str,"%d", answer);
											if (strcmp(str, "0") == 0) printf("Đáp án của bạn: ");
										} while (strcmp(str, "1") != 0 && strcmp(str, "2") != 0 && strcmp(str, "3") != 0 && strcmp(str, "4") != 0 && strcmp(str, "5") != 0);
									} else break;
								}
								break;
							// log out
							case 6:
								recvBytes = recv(sockfd, recvBuff, MAXLINE, 0);
								if (recvBytes == 0) {
									perror("The server terminated prematurely");
									exit(4);
									return 0;
								}
								recvBuff[recvBytes] = '\0';
								printf("%s\n", recvBuff);
							break;
							default: break;
						}
					} while(op_play != 7);
					// pthread_mutex_unlock(&mutex);
				}
				close(sockfd);
			break;
			case 2:
				printf("*****How to play*****\n");
				printf("1. Chon play de dang nhap.\n");
				printf("2. Sau khi dang nhap chon choi online va offline\n");
				printf("3. Neu chon chuc nang choi online, cho khoang 30s \n   de cho nguoi choi thu 2.\n");
				printf("4. 2 nguoi choi se thi dau voi nhau roi chon nguoi \n   ra nguoi chien thang de choi game.\n");			
			break;
			case 3:
				printf("*********Credit*********\n");
				printf("1. Nguyen Thi Thuy Linh\n");
				printf("2. Luong Duc Minh\n");
				printf("3. Nguyen Thanh Ha\n");
			break;
			default: break;
		}
	} while (op != 4);
	return 0;
}


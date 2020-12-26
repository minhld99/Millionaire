#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h> // bzero()
#include <ctype.h> // isdigit(), isalpha()
#include <sys/socket.h> // listen(), accept()
#include <netinet/in.h>
#include <arpa/inet.h> // inet_ntoa(), inet_aton()
#include <unistd.h> // close(sockfd), time_t
#include <pthread.h>
#include <errno.h> 
#include <time.h> // rand()
#include <sqlite3.h>

#define BUFF_SIZE 1000
#define TRUE 1
#define FALSE 0
#define max_clients 100

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

struct user {
    char username[BUFF_SIZE];
    char password[BUFF_SIZE];
    int status; // 0 (blocked), 1 (active), 2 (idle), 3 (online)
    int count; // // number of times of wrong-password's typing
    struct user *next;
};
struct user *head = NULL;

struct client {
    char login_account[BUFF_SIZE];
    int connfd;
    int login_status; // [0: not login yet] & [1: logged in]
    struct client *next;
};
struct client *client1 = NULL;

struct question {
    int index;
    char question[BUFF_SIZE];
    char answerA[BUFF_SIZE];
    char answerB[BUFF_SIZE];
    char answerC[BUFF_SIZE];
    char answerD[BUFF_SIZE];
    int true; // true = {1, 2, 3, 4}
    int used; // used = {0, 1}
};
// struct question questions[BUFF_SIZE];

struct level {
    struct question questions[BUFF_SIZE];
};
struct level questionBank[15];

struct game {
    int room[BUFF_SIZE][BUFF_SIZE]; // room[index][client_sockfd]
    int room_status[BUFF_SIZE]; // room status
    int room_size[BUFF_SIZE]; // number of player in room
    int count; // number of game
    int main_player[BUFF_SIZE]; // main player of each room
    int answer[BUFF_SIZE][BUFF_SIZE]; // answer of each player
    double answer_time[BUFF_SIZE][BUFF_SIZE]; // time for answering of each player
    int true_answer[BUFF_SIZE]; // đáp án cho câu hỏi phụ ở mỗi phòng
};
struct game Game = {0};

pthread_mutex_t mutex;

/* ---------------- Function Declaration ---------------- */

void catch_ctrl_c_and_exit(int sig);
void init();
struct user *newAccount();
struct client *newClient();
void readUserFILE();
void readQuestionFILE();
void addQuestion(int index, int level, char question[BUFF_SIZE], char answerA[BUFF_SIZE], char answerB[BUFF_SIZE], 
                    char answerC[BUFF_SIZE], char answerD[BUFF_SIZE], int true);
void addClient(int connfd);
void addAccount(char username[BUFF_SIZE], char password[BUFF_SIZE], int status);
void deleteClient(int connfd);
int addPlayer2Game(int connfd);
void resetGameRoom(int room_number, int exit_player);
void updateUserFILE(char name[BUFF_SIZE], char new_pass[BUFF_SIZE], int new_status);
void changeClientAccount(char account[BUFF_SIZE], int connfd);
int is_empty(const char *s);
int is_number(const char *s);
void *loginSession(void *client_sock);
void send_to_others(char *mesg, int current_player, int room_number);
char to_ABCD(int i);
void offline5050(int connfd, int i, int j, int k, int true);

/* -------------------------- Main Function -------------------------- */
int main(int argc, char **argv) {
    if (argc < 2) {
        printf("\n\nerror: too few arguments, expected 2, have %d\n", argc);
        printf("\nUsage:\t%s PortNumber\n (For example: %s 5500)\n\n", argv[0], argv[0]);
        exit(-1);
    }
    else if (argc > 2) {
        printf("\n\nerror: too many arguments, expected 2, have %d\n", argc);
        printf("\nUsage:\t%s PortNumber\n (For example: %s 5500)\n\n", argv[0], argv[0]);
        exit(-1);
    }

    if ((!is_number(argv[1])) || atoi(argv[1]) > 65535 || atoi(argv[1]) < 0) {
        printf("\nInvalid port number\n\n");
        exit(-1);
    }

    init();
    readUserFILE();
    readQuestionFILE();
    signal(SIGINT, catch_ctrl_c_and_exit);

    int listenfd, connfd, n, opt = TRUE; 
    const unsigned short SERV_PORT = atoi(argv[1]);
    char buff[BUFF_SIZE] = {0};
    struct sockaddr_in cliaddr, servaddr;
    pthread_t recvt;
    socklen_t clilen;

    listenfd = socket(AF_INET, SOCK_STREAM, 0); //Socket file descriptor used to identify the socket
    if (listenfd < 0) {
        perror("socket() failed");
        exit(-1);
    }

    servaddr.sin_family = AF_INET;
    // servaddr.sin_addr.s_addr = inet_addr("192.168.100.8"); // Minh's macbook internal ipv4 address
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // For running on local
    servaddr.sin_port = htons(SERV_PORT);

    // Allow socket descriptor to be reuseable
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
    // if (setsockopt(listenfd, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt, sizeof(opt)) < 0) {
        perror("setsockopt() failed");
        close(listenfd);
        exit(-1);
    }

    if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("bind() failed");
        close(listenfd);
        exit(-1);
    }
    printf("Server is running... Listener on port %d \n", SERV_PORT);

    if (listen(listenfd, 5) < 0) {
        perror("listen() failed");
        close(listenfd);
        exit(-1);
    }

    clilen = sizeof(cliaddr);
    printf("Waiting for connections ...\n");

    for(;;) {
        if ((connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen)) < 0) {
            perror("Connect error");
            continue;
        }
        printf("--------------\n");
        printf("New connection from [%s:%d] - (%d)\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), connfd);
        addClient(connfd);
        pthread_mutex_lock(&mutex);
        pthread_create(&recvt, NULL, loginSession, &connfd);
        // pthread_join(recvt, NULL); //thread is closed
        pthread_mutex_unlock(&mutex);
    }
    close(listenfd); // close listening socket
    return 0; 
}

/* -------------------------- Utilities --------------------------- */

void catch_ctrl_c_and_exit(int sig) {
    char mesg[] = "\nServer is closing...\n";
    while (client1 != NULL) {
        if (send(client1->connfd, mesg, strlen(mesg), 0) < 0) {
            perror("Send error");
            deleteClient(client1->connfd);
        }
        printf("\nClose socketfd: %d\n", client1->connfd);
        deleteClient(client1->connfd);
    }
    printf("\nBye\n");
    exit(EXIT_SUCCESS);
}

// Create account's text file
void init() {
    FILE *f = fopen("nguoidung.txt", "w");
    fprintf(f, "hust hust123 1\n");
    fprintf(f, "soict soictfit 0\n");
    fprintf(f, "test test123 1\n");
    fprintf(f, "minh minh123 1\n");
    fprintf(f, "ha ha123 1\n");
    fprintf(f, "linh linh123 1\n");
    fclose(f);
}

// Malloc Encapsulation
struct user *newAccount() {
    // Try to allocate user structure.
    struct user *retVal = malloc (sizeof (struct user));
    retVal->next = NULL;
    retVal->count = 1;
    return retVal;
}

struct client *newClient() {
    // Try to allocate user structure.
    struct client *retVal = malloc (sizeof (struct client));
    retVal->next = NULL;
    retVal->login_status = 0;
    return retVal;
}

int addPlayer2Game(int connfd) {
    int room_entered = -1;
    for (int i = 0; i < Game.count+1; i++) {
        if (Game.room_status[i] == 1) {
            Game.room[i][Game.room_size[i]] = connfd;
            Game.room_size[i]++;
            room_entered = i;
        }
    }
    if (room_entered == -1) {
        for (int i = 0; i < BUFF_SIZE; i++) {
            if (Game.room_status[i] == 0) {
                Game.count++;
                Game.room_status[i] = 1;
                Game.room[i][0] = connfd;
                Game.room_size[i]++;
                time_t endwait;
                time_t start = time(NULL);
                time_t seconds = 10; // end loop after this time has elapsed

                endwait = start + seconds;

                printf("start time is: %s", ctime(&start));

                while (start < endwait)
                {
                    if (Game.room_size[i] >= 2) room_entered = i;
                    else room_entered = -1;
                    sleep(1);   // sleep 1s.
                    start = time(NULL);
                    printf("loop time is: %s", ctime(&start));
                }
                printf("end time is: %s", ctime(&endwait));

                if (Game.room_size[i] < 2) { // reset
                    Game.room_status[i] = 0;
                    Game.room[i][0] = 0;
                    Game.room_size[i]--;
                    Game.count--;
                }
                else Game.room_status[i] = 2;
                break;
            }
        }
    }

    return room_entered;
}

int chooseMainPlayer(int room_number, int true_answer) {
    double max = 30;
    int mainPlayer = 0;
    char mesg[BUFF_SIZE];
    if (Game.room_size[room_number] < 2) {
        strcpy(mesg, "\nKhông đủ ít nhất 2 người. Mode online kết thúc!");
        send_to_others(mesg, -1, room_number);
        resetGameRoom(room_number, -1);
        printf("[Room %d]: %s\n", room_number, mesg);
        return 0;
    }
    for (int i = 0; i < Game.room_size[room_number]; i++) {
        if (Game.answer[room_number][i] == true_answer) {
            if (Game.answer_time[room_number][i] < max) {
                max = Game.answer_time[room_number][i];
                mainPlayer = Game.room[room_number][i];
            }
        }
    }
    if (max == 30) {
        for (int i = 0; i < Game.room_size[room_number]; i++) {
            if (Game.answer_time[room_number][i] < max) {
                max = Game.answer_time[room_number][i];
                mainPlayer = Game.room[room_number][i];
            }
        }
    }
    printf("Main Player: %d\n", mainPlayer);
    return mainPlayer;
}

void resetGameRoom(int room_number, int exit_player) {
    if (exit_player == -1) { // End game
        Game.room_status[room_number] = 0;
        memset(Game.room[room_number], 0, Game.count*sizeof(Game.room[room_number]));
        Game.main_player[room_number] = 0;
        Game.room_size[room_number]=0;
        memset(Game.answer[room_number], 0, Game.count*sizeof(Game.answer[room_number]));
        memset(Game.answer_time[room_number], 0, Game.count*sizeof(Game.answer_time[room_number]));
        Game.true_answer[room_number] = 0;
        Game.count--;
        return;
    }
    int number_of_player = Game.room_size[room_number];
    for (int i = 0; i < number_of_player; i++) { // Remove 1 player from room_number
        if (Game.room[room_number][i] == exit_player) {
            Game.room[room_number][i] = Game.room[room_number][number_of_player-1];
            Game.room[room_number][number_of_player-1] = 0;
            Game.room_size[room_number]--;
            Game.answer[room_number][i] = 0;
            Game.answer_time[room_number][i] = 0;
        }
    }
}

// Read from file into linked list
void readUserFILE() {
    char str[100];
    FILE *f = fopen("nguoidung.txt", "r");
    if (f == NULL) {
        printf("Error opening file!\n");   
        exit(1);
    }
    while (fgets(str, sizeof(str), f) != NULL) {
        char *name = strtok(str, " ");
        char *pass = strtok(NULL, " ");
        int status = atoi(strtok(NULL, " "));
        addAccount(name, pass, status);
    }
    fclose(f);
}

void readQuestionFILE() {
    char str[BUFF_SIZE];
    char level[BUFF_SIZE];
    FILE *f = fopen("offline_db.txt", "r");
    if (f == NULL) {
        printf("Error opening file!\n");   
        exit(1);             
    }
    // if ((fgets(str, sizeof(str), f)) == NULL) return;
    for (int i = 0; i < 15; i++) {
        if ((fgets(str, sizeof(str), f)) == NULL) return;
        for (int j = 0; j < 10; j++) {
            char question[BUFF_SIZE];
            if ((fgets(question, sizeof(question), f)) == NULL) return;
            char *offset = strstr(question, "\n");
            if (offset != NULL) *offset = '\0';
            char answerA[BUFF_SIZE];
            if ((fgets(answerA, sizeof(answerA), f)) == NULL) return;
            offset = strstr(answerA, "\n");
            if (offset != NULL) *offset = '\0';
            char answerB[BUFF_SIZE];
            if ((fgets(answerB, sizeof(answerB), f)) == NULL) return;
            offset = strstr(answerB, "\n");
            if (offset != NULL) *offset = '\0';
            char answerC[BUFF_SIZE];
            if ((fgets(answerC, sizeof(answerC), f)) == NULL) return;
            offset = strstr(answerC, "\n");
            if (offset != NULL) *offset = '\0';
            char answerD[BUFF_SIZE];
            if ((fgets(answerD, sizeof(answerD), f)) == NULL) return;
            offset = strstr(answerD, "\n");
            if (offset != NULL) *offset = '\0';
            if ((fgets(str, sizeof(str), f) == NULL)) return;
            offset = strstr(answerD, "\n");
            if (offset != NULL) *offset = '\0';
            int true = atoi(str);
            addQuestion(j, i+1, question, answerA, answerB, answerC, answerD, true);
        }
    }
    fclose(f);
}

void addQuestion(int index, int level, char question[BUFF_SIZE], char answerA[BUFF_SIZE], char answerB[BUFF_SIZE], 
                    char answerC[BUFF_SIZE], char answerD[BUFF_SIZE], int true) {
    
    strcpy(questionBank[level-1].questions[index].question, question);
    strcpy(questionBank[level-1].questions[index].answerA, answerA);
    strcpy(questionBank[level-1].questions[index].answerB, answerB);
    strcpy(questionBank[level-1].questions[index].answerC, answerC);
    strcpy(questionBank[level-1].questions[index].answerD, answerD);
    questionBank[level-1].questions[index].true = true;
    questionBank[level-1].questions[index].used = 0;
}

// Add client to linked list
void addClient(int connfd) {
    struct client *new_client = newClient();
    // strcpy(new_client->login_account, account);
    new_client->connfd = connfd;
    if (client1 == NULL) client1 = new_client; // if linked list is empty
    else {
        struct client *tmp = client1; // assign head to p 
        while (tmp->next != NULL) tmp = tmp->next; // traverse the list until the last node
        tmp->next = new_client; //Point the previous last node to the new node created.
    }
}

// Add account to linked list
void addAccount(char username[BUFF_SIZE], char password[BUFF_SIZE], int status) {
    struct user *new_user = newAccount();
    strcpy(new_user->username, username);
    strcpy(new_user->password, password);
    new_user->status = status;
    if (head == NULL) head = new_user; // if linked list is empty
    else {
        struct user *tmp = head; // assign head to p 
        while (tmp->next != NULL) tmp = tmp->next; // traverse the list until the last node
        tmp->next = new_user; //Point the previous last node to the new node created.
    }
}

void deleteClient(int connfd) { 
    close(connfd);
    struct client *temp = client1;
    struct client *prev = NULL; 
    // If head node itself holds the key to be deleted
    if (temp != NULL && temp->connfd == connfd) {
        updateUserFILE(temp->login_account, "", 1);
        client1 = temp->next;   // Changed head 
        free(temp);               // free old head 
        return; 
    } 
    // Search for the key to be deleted, keep track of the 
    // previous node as we need to change 'prev->next' 
    while (temp != NULL && temp->connfd != connfd) { 
        prev = temp;
        temp = temp->next; 
    } 
    // If key was not present in linked list 
    if (temp == NULL) return; 
    // Unlink the node from linked list 
    updateUserFILE(temp->login_account, "", 1);
    prev->next = temp->next; 
    free(temp);  // Free memory
}

// Change password or change status and save to file
void updateUserFILE(char name[BUFF_SIZE], char new_pass[BUFF_SIZE], int new_status) {
    FILE *f = fopen("nguoidung.txt", "w");
    if (f == NULL) {
        printf("Error opening file!\n");   
        exit(1);             
    }
    struct user *tmp = head;
    while (tmp != NULL) {  
        if (strcmp(tmp->username, name) == 0) {
            // memcpy(tmp->password, new_pass, BUFF_SIZE);
            if (strcmp(new_pass, "") != 0) strcpy(tmp->password, new_pass);
            if (new_status != 4) tmp->status = new_status;
        }
        fprintf(f, "%s %s %d\n", tmp->username, tmp->password, tmp->status);
        tmp = tmp->next;
    }
    fclose(f);
}

// Change client's login account
void changeClientAccount(char account[BUFF_SIZE], int connfd) {
    struct client *tmp = client1;
    while (tmp != NULL) {  
        if (tmp->connfd == connfd) {
            strcpy(tmp->login_account, account);
        }
        tmp = tmp->next;
    }
}

int is_empty(const char *s) {
    while (*s != '\0') {
        if (!isspace((unsigned char)*s))
            return 0; // string is not empty
        s++;
    }
    return 1; // string is empty
}

int is_number(const char *s) {
    while (*s != '\0') {
        if (!isdigit((unsigned char)*s))
            return 0; // string is not number
        s++;
    }
    return 1; // string is number
}

void *loginSession(void *client_sock) {
	int connfd = *((int *)client_sock);
    char buff[BUFF_SIZE] = {0};
    struct client *cli = client1;
    while (cli->connfd != connfd && cli != NULL) cli = cli->next;
	int n = 0;
	while((n = recv(connfd, buff, BUFF_SIZE, 0)) > 0) {
        buff[n] = '\0';
        printf(CYN "[%d]: %s\n" RESET, connfd, buff);
        char mesg[BUFF_SIZE] = {0};
        // Login
        if (strcmp(buff, "1") == 0) {
            if (cli->login_status == 0) {
                strcpy(mesg, "Nhập tên: ");
                if (send(connfd, mesg, strlen(mesg), 0) < 0){
                    perror("Send error");
                    deleteClient(connfd);
                    return NULL;
                }
                if ((n = recv(connfd, buff, BUFF_SIZE, 0)) <= 0) {
                    printf("Host disconnected: socket_fd(%d)\n", connfd);
                    deleteClient(connfd);
                    return NULL;
                }
                buff[n] = '\0';
                printf(CYN "[%d]: %s\n" RESET, cli->connfd, buff);
                struct user *tmp = head;
                while (tmp != NULL) {
                    if (strcmp(tmp->username, buff) == 0) {
                        strcpy(mesg, "Nhập mật khẩu: ");
                        if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                            perror("Send error");
                            deleteClient(connfd);
                            return NULL;
                        }
                        if ((n = recv(connfd, buff, BUFF_SIZE, 0)) <= 0) {
                            printf("Host disconnected: socket_fd(%d)\n", connfd);
                            deleteClient(connfd);
                            return NULL;
                        }
                        buff[n] = '\0';
                        printf(CYN "[%d]: %s\n" RESET, cli->connfd, buff);
                        if (strcmp(tmp->password, buff) == 0) {
                            if (tmp->status == 0 || tmp->status == 2) {
                                strcpy(mesg, "Tài khoản chưa sẵn sàng");
                                if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                                    perror("Send error");
                                    deleteClient(connfd);
                                    return NULL;
                                }
                            } 
                            if (tmp->status == 3) {
                                strcpy(mesg, "Lỗi! Tài khoản đang đăng nhập trên thiết bị khác.\nNhập lại tên: ");
                                if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                                    perror("Send error");
                                    deleteClient(connfd);
                                    return NULL;
                                }
                            }
                            else {
                                tmp->count = 1;
                                strcpy(mesg, "OK");
                                updateUserFILE(tmp->username, "", 3);
                                changeClientAccount(tmp->username, connfd);
                                if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                                    perror("Send error");
                                    deleteClient(connfd);
                                    return NULL;
                                }
                                cli->login_status = 1;
                            }
                        }
                        else {
                            if (tmp->count == 3) {
                                updateUserFILE(tmp->username, tmp->password, 0); // 0 : blocked
                                strcpy(mesg, "Tài khoản đang bị khóa. Đăng nhập thất bại");
                                printf("%s\n", mesg);
                                if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                                    perror("Send error");
                                    deleteClient(connfd);
                                    return NULL;
                                }
                            }
                            else {
                                strcpy(mesg, "Sai mật khẩu. Đăng nhập thất bại");
                                tmp->count++;
                                if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                                    perror("Send error");
                                    deleteClient(connfd);
                                    return NULL;
                                }
                            }
                        }
                        break;
                    }
                    tmp = tmp->next;
                }
                if (tmp == NULL) {
                    strcpy(mesg, "Tài khoản không tồn tại! Đăng ký mới?");
                    if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                        perror("Send error");
                        deleteClient(connfd);
                        return NULL;
                    }
                    continue;
                }
            }
            else if (cli->login_status == 1) {
                sprintf(mesg, "Bạn đang đăng nhập bằng tài khoản %s.", cli->login_account);
                if (send(connfd, mesg, strlen(mesg), 0) < 0){
                    perror("Send error");
                    deleteClient(connfd);
                    return NULL;
                }
            }
        }

        // Register
        else if (strcmp(buff, "2") == 0) {
            if (cli->login_status == 0) {
                char name[BUFF_SIZE] = {0};
                char pass[BUFF_SIZE] = {0};
                strcpy(mesg, "Nhập tên: ");
                if (send(connfd, mesg, strlen(mesg), 0) < 0){
                    perror("Send error");
                    deleteClient(connfd);
                    return NULL;
                }
                if ((n = recv(connfd, buff, BUFF_SIZE, 0)) <= 0) {
                    printf("Host disconnected: socket_fd(%d)\n", connfd);
                    deleteClient(connfd);
                    return NULL;
                }
                buff[n] = '\0';
                printf(CYN "[%d]: %s\n" RESET, cli->connfd, buff);
                strcpy(mesg, "Nhập mật khẩu: ");
                struct user *tmp = head;
                while (tmp != NULL) {  
                    if (strcmp(tmp->username, buff) == 0) {
                        strcpy(mesg, "Tên tài khoản đã tồn tại! Hãy chọn tên khác.");
                        break;
                    }
                    tmp = tmp->next;  
                }
                if (send(connfd, mesg, strlen(mesg), 0) < 0){
                    perror("Send error");
                    deleteClient(connfd);
                    return NULL;
                }
                if (strcmp(mesg, "Nhập mật khẩu: ") != 0) continue;
                else strcpy(name, buff);
                if ((n = recv(connfd, buff, BUFF_SIZE, 0)) <= 0) {
                    printf("Host disconnected: socket_fd(%d)\n", connfd);
                    deleteClient(connfd);
                    return NULL;
                }
                strcpy(pass, buff);
                addAccount(name, pass, 1);
                updateUserFILE(name, pass, 1);
                strcpy(mesg, "Đăng ký thành công!");
                if (send(connfd, mesg, strlen(mesg), 0) < 0){
                    perror("Send error");
                    deleteClient(connfd);
                    return NULL;
                }
            }
            else if (cli->login_status == 1) {
                sprintf(mesg, "Bạn đang đăng nhập bằng tài khoản %s. Hãy đăng xuất trước.", cli->login_account);
                if (send(connfd, mesg, strlen(mesg), 0) < 0){
                    perror("Send error");
                    deleteClient(connfd);
                    return NULL;
                }
            }
        }

        // Change password
        else if (strcmp(buff, "3") == 0) {
            if (cli->login_status == 0) {
                strcpy(mesg, "Bạn chưa đăng nhập, không thể đổi mật khẩu.");
                if (send(connfd, mesg, strlen(mesg), 0) < 0){
                    perror("Send error");
                    deleteClient(connfd);
                    return NULL;
                }
            }
            else if (cli->login_status == 1) {
                // receive new password
                strcpy(mesg, "Nhập mật khẩu mới: ");
                if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                    perror("Send error");
                    deleteClient(connfd);
                    break;
                }
                if ((n = recv(connfd, buff, BUFF_SIZE, 0)) <= 0) {
                    printf("Host disconnected: socket_fd(%d)\n", connfd);
                    deleteClient(connfd);
                    return NULL;
                }
                buff[n] = '\0';
                printf(CYN "[%d - %s]: %s\n" RESET, cli->connfd, cli->login_account, buff);
                int i, alphabet = 0, number = 0; 
                char alpha[BUFF_SIZE] = {0}, digit[BUFF_SIZE] = {0}; 
                for (i = 0; buff[i]!= '\0'; i++) { 
                    // check for alphabets 
                    if (isalpha(buff[i]) != 0) {
                        alpha[alphabet] = buff[i];
                        alphabet++; 
                    }
                    // check for decimal digits 
                    else if (isdigit(buff[i]) != 0) {
                        digit[number] = buff[i];
                        number++; 
                    }
                    else {
                        alphabet = 0; number = 0;
                        strcpy(mesg, "Error");
                        if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                            perror("Send error");
                            deleteClient(connfd);
                        }
                        break;
                    }
                }
                if (alphabet != 0 || number != 0) {
                    strcpy(mesg, "Đổi mật khẩu thành công!\nMật khẩu mới (đã được mã hóa) là: ");
                    strcat(mesg, digit);
                    strcat(mesg, alpha);
                    if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                        perror("Send error");
                        deleteClient(connfd);
                        break;
                    }
                    updateUserFILE(cli->login_account, buff, 4); // 4 ~ keep the current status
                }
            }
        }

        // Play online
        else if (strcmp(buff, "4") == 0) {
            if (cli->login_status == 0) {
                strcpy(mesg, "Cần đăng nhập trước khi chơi!");
                if (send(connfd, mesg, strlen(mesg), 0) < 0){
                    perror("Send error");
                    deleteClient(connfd);
                    return NULL;
                }
            }
            else if (cli->login_status == 1) {
                strcpy(mesg, "Đang kiểm tra, chờ chút...\n");
                if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                    perror("Send error");
                    deleteClient(connfd);
                    break;
                }
                int room_entered = addPlayer2Game(connfd);
                Game.main_player[room_entered] = 0; // uid_socketfd of main player
                if (room_entered == -1) {
                    sprintf(mesg, "Player [%d - %s] enter room failed!", cli->connfd, cli->login_account);
                    printf(CYN "%s\n" RESET, mesg);
                    strcpy(mesg, "Không đủ người chơi online. Hãy thử lại sau!");
                    if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                        perror("Send error");
                        deleteClient(connfd);
                        break;
                    }
                    continue;
                }
                else {
                    while (1) if (Game.room_status[room_entered] == 2) break;
                    sprintf(mesg, "Player [%d - %s] enter room %d.", cli->connfd, cli->login_account, room_entered);
                    printf(CYN "%s\n" RESET, mesg);
                    sprintf(mesg, "[Phòng %d] Ghép cặp thành công. Bắt đầu!", room_entered);
                    if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                        sprintf(mesg, "Người chơi [%d - %s] đã thoát.", cli->connfd, cli->login_account);
                        send_to_others(mesg, connfd, room_entered);
                        resetGameRoom(room_entered, connfd);
                        printf("[Room %d]: %s\n", room_entered, mesg);
                        perror("Send error");
                        deleteClient(connfd);
                        break;
                    }
                }
                // Choose main player
                sqlite3 *db;
                char *err_msg = 0;
                sqlite3_stmt *res;
                int rc = sqlite3_open("Question", &db);
                if (rc != SQLITE_OK) {
                    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
                    sqlite3_close(db);
                    continue;
                }
                if (connfd == Game.room[room_entered][0]) {
                    char sql[BUFF_SIZE];
                    sprintf(sql, "SELECT * FROM Question ORDER BY RANDOM() LIMIT 1");
                    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
                    if (rc != SQLITE_OK) {  
                        fprintf(stderr, "Failed to select data\n");
                        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
                        sqlite3_close(db);
                        continue;
                    }
                    int step = sqlite3_step(res);
                    if (step == SQLITE_ROW) { 
                        sprintf(mesg, "Câu hỏi phụ: %s\nA. %s\nB. %s\nC. %s\nD. %s\nĐáp án của bạn: ",
                        sqlite3_column_text(res, 0),
                        sqlite3_column_text(res, 3),
                        sqlite3_column_text(res, 4),
                        sqlite3_column_text(res, 5),
                        sqlite3_column_text(res, 6));
                        send_to_others(mesg, -1, room_entered);
                        printf("%s\n", mesg);
                        Game.true_answer[room_entered] = atoi((char *)sqlite3_column_text(res, 7));
                    }
                }
                if ((n = recv(connfd, buff, BUFF_SIZE, 0)) <= 0) {
                    sprintf(mesg, "Người chơi [%d - %s] đã thoát.", cli->connfd, cli->login_account);
                    send_to_others(mesg, connfd, room_entered);
                    printf("[Room %d]: %s\n", room_entered, mesg);
                    resetGameRoom(room_entered, connfd);
                    printf("Host disconnected: socket_fd(%d)\n", connfd);
                    deleteClient(connfd);
                    return NULL;
                }

                buff[n] = '\0';
                sprintf(mesg,"[%d - %s]: %s\n", cli->connfd, cli->login_account, buff);
                printf(CYN "%s" RESET, mesg);
                char *answer = strtok(buff, " ");
                double time = atof(strtok(NULL, " "));
    
                if (atoi(answer) == Game.true_answer[room_entered]) strcpy(mesg, "Chính xác!\nChờ kết quả từ chương trình...");
                else sprintf(mesg, "Sai!! Đáp án đúng là %c\nChờ kết quả từ chương trình...\n", to_ABCD(Game.true_answer[room_entered]));
                if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                    sprintf(mesg, "Người chơi [%d - %s] đã thoát.", cli->connfd, cli->login_account);
                    send_to_others(mesg, connfd, room_entered);
                    printf("[Room %d]: %s\n", room_entered, mesg);
                    resetGameRoom(room_entered, connfd);
                    perror("Send error");
                    deleteClient(connfd);
                    break;
                }
                
                printf("Số người trong phòng: %d\n", Game.room_size[room_entered]);
                while (1) {
                    int loop = 0;
                    for (int j = 0; j < Game.room_size[room_entered]; j++) {
                        if (Game.room[room_entered][j] == connfd) {
                            Game.answer[room_entered][j] = atoi(answer);
                            Game.answer_time[room_entered][j] = time;
                        }
                        else if (Game.answer[room_entered][j] == 0) {
                            loop = 1;
                            break;
                        }
                    }
                    if (loop == 1) continue;
                    break;
                }
                Game.main_player[room_entered] = chooseMainPlayer(room_entered, Game.true_answer[room_entered]);
                if (Game.main_player[room_entered] == 0) continue;

                // Spectators
                if (connfd != Game.main_player[room_entered]) {
                    while (Game.room_status[room_entered] != 0) {
                        if ((n = recv(connfd, buff, strlen(buff), MSG_DONTWAIT)) <= 0) {
                            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                                sprintf(mesg, "Người xem [%d - %s] đã thoát.\n", cli->connfd, cli->login_account);
                                send_to_others(mesg, connfd, room_entered);
                                resetGameRoom(room_entered, connfd);
                                printf("Host disconnected: socket_fd(%d)\n", connfd);
                                deleteClient(connfd);
                                return NULL;
                            }
                        }
                    }
                    continue;
                }
                // Main player only
                else {
                    struct client *tmp = client1;
                    while (tmp->connfd != Game.main_player[room_entered] && tmp != NULL) tmp = tmp->next;
                    sprintf(mesg, "Người chơi: " CYN "%s" RESET "\nKhán giả:", tmp->login_account);
                    for (int j = 0; j < Game.room_size[room_entered]; j++) {
                        char str[BUFF_SIZE] = {0};
                        tmp = client1;
                        if (Game.room[room_entered][j] != Game.main_player[room_entered]) {
                            while (tmp->connfd != Game.room[room_entered][j] && tmp != NULL) tmp = tmp->next;
                            sprintf(str, CYN " %s" RESET, tmp->login_account);
                            strcat(mesg, str);
                        }
                    }
                    strcat(mesg, "\n");
                    send_to_others(mesg, -1, room_entered);
                    
                    int i, j = 3, k, help;
                    for (i = 0; i < 15; i++) {
                        if (help == 0) {
                            char sql[BUFF_SIZE];
                            sprintf(sql, "SELECT * FROM Question%d ORDER BY RANDOM() LIMIT 1", i+1);
                            rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
                            if (rc != SQLITE_OK) {  
                                fprintf(stderr, "Failed to select data\n");
                                fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
                                sqlite3_close(db);
                                continue;
                            }
                            int step = sqlite3_step(res);
                            if (step == SQLITE_ROW) { 
                                char tmp[BUFF_SIZE] = {0};
                                    sprintf(tmp, "Câu hỏi %d: %s\nA. %s\nB. %s\nC. %s\nD. %s\nĐáp án của bạn: ", i+1,
                                    sqlite3_column_text(res, 1),
                                    sqlite3_column_text(res, 2),
                                    sqlite3_column_text(res, 3),
                                    sqlite3_column_text(res, 4),
                                    sqlite3_column_text(res, 5));
                                    if (i > 0) sprintf(mesg, "Chính xác!\n\n%s", tmp);
                                    else strcpy(mesg, tmp);
                                    send_to_others(mesg, -1, room_entered);
                                    printf("%s\n", mesg);
                            }
                        }
                        help = 0;
                        if ((n = recv(connfd, buff, BUFF_SIZE, 0)) <= 0) {
                            sprintf(mesg, "Người chơi chính [%d - %s] đã thoát.\nMode online kết thúc!", cli->connfd, cli->login_account);
                            send_to_others(mesg, connfd, room_entered);
                            printf("[Room %d]: %s\n", room_entered, mesg);
                            resetGameRoom(room_entered, -1);
                            printf("Host disconnected: socket_fd(%d)\n", connfd);
                            deleteClient(connfd);
                            return NULL;
                        }
                        buff[n] = '\0';
                        char *answer = strtok(buff, " ");
                        sprintf(mesg,"[%d - %s]: %c\n", cli->connfd, cli->login_account, to_ABCD(atoi(answer)));
                        printf(CYN "%s" RESET, mesg);
                        send_to_others(mesg, connfd, room_entered);
                        if (atoi(answer) == 5) {
                            // pthread_mutex_lock(&mutex);
                            help = 1; i--;
                            if (j == 0) {
                                sprintf(mesg, "Đã hết số lượt sử dụng sự trợ giúp.\nĐáp án của bạn: ");
                                printf("%s\n", mesg);
                                send_to_others(mesg, -1, room_entered);
                            }
                            else {
                                j--;
                                do k = (rand() % (4 - 1 + 1)) + 1; // (upper - lower + 1)) + lower
                                while (k == atoi((char *)sqlite3_column_text(res, 6)));
                                
                                char option1[BUFF_SIZE], option2[BUFF_SIZE];

                                if (k == 1) strcpy(option1, (char *)sqlite3_column_text(res, 2));
                                else if (k == 2) strcpy(option1, (char *)sqlite3_column_text(res, 3));
                                else if (k == 3) strcpy(option1, (char *)sqlite3_column_text(res, 4));
                                else strcpy(option1, (char *)sqlite3_column_text(res, 5));

                                int true = atoi((char *)sqlite3_column_text(res, 6));
                                if (true == 1) strcpy(option2, (char *)sqlite3_column_text(res, 2));
                                else if (true == 2) strcpy(option2, (char *)sqlite3_column_text(res, 3));
                                else if (true == 3) strcpy(option2, (char *)sqlite3_column_text(res, 4));
                                else strcpy(option2, (char *)sqlite3_column_text(res, 5));

                                if (k < true)
                                    sprintf(mesg, "Bạn đã sử dụng sự trợ giúp 50/50 " CYN "(Còn %d lượt)" RESET "\nCâu hỏi %d: %s\n%c. %s\n%c. %s\nĐáp án của bạn: ",
                                            j, i+1, sqlite3_column_text(res, 1), 
                                            to_ABCD(k), option1, to_ABCD(true), option2);
                                else
                                    sprintf(mesg, "Bạn đã sử dụng sự trợ giúp 50/50 " CYN "(Còn %d lượt)" RESET "\nCâu hỏi %d: %s\n%c. %s\n%c. %s\nĐáp án của bạn: ",
                                            j, i+1, sqlite3_column_text(res, 1), 
                                            to_ABCD(true), option2, to_ABCD(k), option1);
                                printf("%s\n", mesg);
                                send_to_others(mesg, -1, room_entered);
                            }
                            // pthread_mutex_unlock(&mutex);
                        }
                        else if (strcmp(answer, (char *)sqlite3_column_text(res, 6)) != 0) {
                            if (i == 0) sprintf(mesg, "Sai! Đáp án đúng là %c\nKém quá, bạn chưa trả lời đúng câu nào 🤣 🤣\n", to_ABCD(atoi((char *)sqlite3_column_text(res, 6))));
                            else sprintf(mesg, "Sai! Đáp án đúng là %c\nBạn đã trả lời đúng %d câu hỏi.\n", to_ABCD(atoi((char *)sqlite3_column_text(res, 6))), i);
                            printf("%s\n", mesg);
                            send_to_others(mesg, -1, room_entered);
                            break;
                        }
                    }
                    if (i == 15) {
                        strcpy(mesg, "\nChúc mừng bạn đã trả lời đúng 15 câu hỏi! ️🎉️ 🎉\n\n");
                        printf("%s\n", mesg);
                        send_to_others(mesg, -1, room_entered);
                    }
                    sqlite3_finalize(res);
                    sqlite3_close(db);
                }
                resetGameRoom(room_entered, -1);
            }
        }

        // Play offline
        else if (strcmp(buff, "5") == 0) {
            if (cli->login_status == 0) {
                strcpy(mesg, "Cần đăng nhập trước khi chơi!");
                if (send(connfd, mesg, strlen(mesg), 0) < 0){
                    perror("Send error");
                    deleteClient(connfd);
                    return NULL;
                }
            }
            else if (cli->login_status == 1) {
                srand(time(0));
                int i, j, k, help;
                for (i = 0; i < 15; i++) {
                    char tmp[BUFF_SIZE] = {0};
                    if (help == 0) { 
                        j = (rand() % (9 - 0 + 1)) + 0;
                        sprintf(tmp, "Câu hỏi %d: %s\nA. %s\nB. %s\nC. %s\nD. %s\nĐáp án của bạn: ", i+1,
                        questionBank[i].questions[j].question, 
                        questionBank[i].questions[j].answerA, 
                        questionBank[i].questions[j].answerB, 
                        questionBank[i].questions[j].answerC, 
                        questionBank[i].questions[j].answerD);
                        if (i == 15) strcpy(mesg, "\nChúc mừng bạn đã trả lời đúng 15 câu hỏi!\n\n");
                        else if (i != 0) sprintf(mesg, "Chính xác!\n\n%s", tmp);
                        else strcpy(mesg, tmp);
                        printf("%s\n", mesg);
                        if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                            perror("Send error");
                            deleteClient(connfd);
                            return NULL;
                        }
                    }
                    help = 0;
                    if ((n = recv(connfd, buff, BUFF_SIZE, 0)) <= 0) {
                        printf("Host disconnected: socket_fd(%d)\n", connfd);
                        deleteClient(connfd);
                        return NULL;
                    }
                    buff[n] = '\0';
                    printf(CYN "[%d - %s]: %s\n" RESET, cli->connfd, cli->login_account, buff);
                    if (atoi(buff) == 5) {
                        help = 1;
                        do k = (rand() % (4 - 1 + 1)) + 1; // (upper - lower + 1)) + lower
                        while (k == questionBank[i].questions[j].true);
                        offline5050(connfd, i, j, k, questionBank[i].questions[j].true);
                        i--;
                    }
                    else if (atoi(buff) != questionBank[i].questions[j].true) {
                        if (i == 0) sprintf(mesg, "Sai! Đáp án đúng là %c\nKém quá, bạn chưa trả lời đúng câu nào 🤣 🤣\n", to_ABCD(questionBank[i].questions[j].true));
                        else sprintf(mesg, "Sai! Đáp án đúng là %c\nBạn đã trả lời đúng %d câu hỏi.\n", to_ABCD(questionBank[i].questions[j].true), i);
                        printf("%s\n", mesg);
                        if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                            perror("Send error");
                            deleteClient(connfd);
                        }
                        break;
                    }
                }
                if (i == 15) {
                    strcpy(mesg, "\nChúc mừng bạn đã trả lời đúng 15 câu hỏi! ️🎉️ 🎉\n\n");
                    if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                        perror("Send error");
                        deleteClient(connfd);
                        break;
                    }
                }
            }
        }

        // Log out
        else if (strcmp(buff, "6") == 0) {
            if (cli->login_status == 0) {
                strcpy(mesg, "Bạn chưa đăng nhập, không cần đăng xuất.");
                if (send(connfd, mesg, strlen(mesg), 0) < 0){
                    perror("Send error");
                    deleteClient(connfd);
                    return NULL;
                }
            }
            else if (cli->login_status == 1) {
                strcpy(mesg, "Goodbye ");
                strcat(mesg, cli->login_account);
                updateUserFILE(cli->login_account, "", 1);
                changeClientAccount("", connfd);
                cli->login_status = 0;
                if (send(connfd, mesg, strlen(mesg), 0) < 0) {
                    perror("Send error");
                    deleteClient(connfd);
                    break;
                }
            }
        }
    }
    if (n <= 0) {
        printf("Host disconnected: socket_fd(%d)\n", connfd);
        deleteClient(connfd);
    }
    return 0;
}

void send_to_others(char *mesg, int current_player, int room_number){
	pthread_mutex_lock(&mutex);
    for (int i = 0; i < Game.room_size[room_number]; i++) {
        if (Game.room[room_number][i] == current_player) continue;
        if (send(Game.room[room_number][i], mesg, strlen(mesg), 0) < 0) {
            perror("Send error");
            deleteClient(Game.room[room_number][i]);
        }
    }
	pthread_mutex_unlock(&mutex);
}

char to_ABCD(int i) {
    if (i == 1) return 'A';
    else if (i == 2) return 'B';
    else if (i == 3) return 'C';
    else if (i == 4) return 'D';
    else if (i == 5) return 'H';
    else return ' ';
}

void offline5050(int connfd, int i, int j, int k, int true) {
    pthread_mutex_lock(&mutex);
    char tmp[BUFF_SIZE], option1[BUFF_SIZE], option2[BUFF_SIZE];
    
    if (k == 1) strcpy(option1, questionBank[i].questions[j].answerA);
    else if (k == 2) strcpy(option1, questionBank[i].questions[j].answerB);
    else if (k == 3) strcpy(option1, questionBank[i].questions[j].answerC);
    else strcpy(option1, questionBank[i].questions[j].answerD);

    if (true == 1) strcpy(option2, questionBank[i].questions[j].answerA);
    else if (true == 2) strcpy(option2, questionBank[i].questions[j].answerB);
    else if (true == 3) strcpy(option2, questionBank[i].questions[j].answerC);
    else strcpy(option2, questionBank[i].questions[j].answerD);

    if (k < true) {
        sprintf(tmp, "Bạn đã sử dụng sự trợ giúp 50/50.\nCâu hỏi %d: %s\n%c. %s\n%c. %s\nĐáp án của bạn: ", 
                i+1, questionBank[i].questions[j].question, 
                to_ABCD(k), option1, to_ABCD(true), option2);
    }
    else {
        sprintf(tmp, "Bạn đã sử dụng sự trợ giúp 50/50.\nCâu hỏi %d: %s\n%c. %s\n%c. %s\nĐáp án của bạn: ", 
                i+1, questionBank[i].questions[j].question, 
                to_ABCD(true), option2, to_ABCD(k), option1);
    }

    printf("%s\n", tmp);
    if (send(connfd, tmp, strlen(tmp), 0) < 0) {
        perror("Send error");
        deleteClient(connfd);
    }
    pthread_mutex_unlock(&mutex);
}

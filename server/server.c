#include <stdio.h>
#include <stdlib.h>
#include <string.h> // bzero()
#include <ctype.h> // isdigit(), isalpha()
#include <sys/socket.h> // listen(), accept()
#include <netinet/in.h>
#include <arpa/inet.h> // inet_ntoa(), inet_aton()
#include <unistd.h> // close(sockfd)
#include <pthread.h>
#include <errno.h> 
#include <time.h> // rand()

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
    int status; // 0 (blocked), 1 (active), 2 (idle)
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

char online_user[BUFF_SIZE]; // Who are online?
pthread_mutex_t mutex;

/* ---------------- Function Declaration ---------------- */

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

void updateUserFILE(char name[BUFF_SIZE], char new_pass[BUFF_SIZE], int new_status);
void changeClientAccount(char account[BUFF_SIZE], int connfd);
int is_empty(const char *s);
int is_number(const char *s);
void *loginSession(void *client_sock);
void send_to_others(char *mesg, int curr);

/* -------------------------- Main Function -------------------------- */
int main(int argc, char **argv) {
    if (argc < 2) {
        printf("error: too few arguments, expected 2, have %d\n", argc);
        printf("Usage:\t%s PortNumber\n (For example: %s 5500)\n", argv[0], argv[0]);
        exit(-1);
    }
    else if (argc > 2) {
        printf("error: too many arguments, expected 2, have %d\n", argc);
        printf("Usage:\t%s PortNumber\n (For example: %s 5500)\n", argv[0], argv[0]);
        exit(-1);
    }

    if ((!is_number(argv[1])) || atoi(argv[1]) > 65535 || atoi(argv[1]) < 0) {
        printf("Invalid port number\n");
        exit(-1);
    }

    init();
    readUserFILE();
    readQuestionFILE();
    int listenfd, connfd, n, client_socket[max_clients], opt = TRUE, activity, i, j, k, rc, maxfd, current_size = 0;
    const unsigned short SERV_PORT = atoi(argv[1]);
    char buff[BUFF_SIZE] = {0};
    struct sockaddr_in cliaddr, servaddr;
    pthread_t recvt;
    pid_t childpid;
    socklen_t clilen;
    fd_set readfds;
    // struct timeval timeout;

    // for (i = 0; i < 15; i++) {
    //     for (j = 0; j < 10; j++) {
    //         printf("(%s)\n(%s)\n(%s)\n(%s)\n(%s)\n(%d)\n", 
    //         questionBank[i].questions[j].question, 
    //         questionBank[i].questions[j].answerA, 
    //         questionBank[i].questions[j].answerB, 
    //         questionBank[i].questions[j].answerC, 
    //         questionBank[i].questions[j].answerD, 
    //         questionBank[i].questions[j].true);
    //     }
    // }

    listenfd = socket(AF_INET, SOCK_STREAM, 0); //Socket file descriptor used to identify the socket
    if (listenfd < 0) {
        perror("socket() failed");
        exit(-1);
    }

    servaddr.sin_family = AF_INET;
    // servaddr.sin_addr.s_addr = inet_addr("192.168.100.15"); // Minh's macbook internal ipv4 address
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // For running on local
    servaddr.sin_port = htons(SERV_PORT);

    for (i = 0; i < max_clients; i++) client_socket[i] = -1;

    // Allow socket descriptor to be reuseable
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0) {
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
        // clear the socket set
        FD_ZERO(&readfds);

        // add master socket to set
        FD_SET(listenfd, &readfds);
        maxfd = listenfd;

        // add child sockets to set
        for (i = 0 ; i < max_clients ; i++) {   
			//if valid socket descriptor then add to read list
			if(client_socket[i] > 0) FD_SET(client_socket[i], &readfds);
            
            //highest file descriptor number, need it for the select function
            if(client_socket[i] > maxfd) maxfd = client_socket[i];;
        }

        // timeout.tv_sec = 3;
        // timeout.tv_usec = 500000; // (microsecond) = 0.5s

        // wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select(maxfd+1, &readfds, NULL, NULL, NULL);
   
        if ((activity < 0) && (errno!=EINTR)) printf("select error");

        // If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(listenfd, &readfds)) {
            if ((connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
         
            // inform user of socket number - used in send and receive commands
            printf("New connection from [%s:%d] - (%d)\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), connfd);
            current_size++;
             
            // add new socket to array of sockets
            for (i = 0; i < max_clients; i++) {
                // if position is empty
				if(client_socket[i] == -1) {
                    client_socket[i] = connfd;
                    printf("Adding to list of sockets as %d\n", i);
                    addClient(connfd);
					break;
                }
            }
        }
        // else its some IO operation on some other socket
        for (i = 0; i < current_size; i++) {
            if (FD_ISSET(client_socket[i], &readfds)) {
                // Check if it was for closing , and also read the incoming message
                printf("--------------\n");
                printf("Message from Socket[%d]...\n", client_socket[i]);
                if ((rc = read(client_socket[i], buff, sizeof(buff))) == 0) {
                    // Somebody disconnected , get his details and print
                    printf("Host disconnected: socket_fd(%d)\n", client_socket[i]);
                    
                    // Close the socket and mark as 0 in list for reuse
                    close(client_socket[i]);
                    deleteClient(client_socket[i]);
                    client_socket[i] = -1;
                    current_size--;
                }
                
                // Echo back
                else {
                    // pthread_mutex_lock(&mutex);
                    pthread_create(&recvt, NULL, loginSession, &client_socket[i]);
                    pthread_join(recvt, NULL); //thread is closed
                    // pthread_mutex_unlock(&mutex);
                }
            }
        }
    }
    // close(listenfd); // close listening socket
    return 0; 
}

/* -------------------------- Utilities --------------------------- */

// Create account's text file
void init() {
    FILE *f = fopen("nguoidung.txt", "w");
    fprintf(f, "hust hust123 1\n");
    fprintf(f, "soict soictfit 0\n");
    fprintf(f, "test test123 1\n");
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
            // printf("%s\nA. %s\nB. %s\nC. %s\nD. %s\n", question, answerA, answerB, answerC, answerD);
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
    struct client *temp = client1;
    struct client *prev = NULL; 
    // If head node itself holds the key to be deleted
    if (temp != NULL && temp->connfd == connfd) { 
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
            strcpy(tmp->password, new_pass);
            if (new_status != 3) tmp->status = new_status;
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
            // memcpy(tmp->login_account, account, BUFF_SIZE);
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
	int n;
    strcpy(buff, "Enter username: ");
    send(connfd, buff, strlen(buff), 0);
	while((n = recv(connfd, buff, BUFF_SIZE, 0)) > 0) {
        struct client *cli = client1;
        while (cli->connfd != connfd && cli != NULL) cli = cli->next;
        char mesg[BUFF_SIZE] = {0};
        buff[n] = '\0';
        printf(CYN "[%d]: %s\n" RESET, connfd, buff);
        if (cli->login_status == 0) {
            struct user *tmp = head;
            while (tmp != NULL) {
                if (strcmp(tmp->username, buff) == 0) {
                    strcpy(mesg, "Insert password: ");
                    send(connfd, mesg, strlen(mesg), 0);
                    n = recv(connfd, buff, BUFF_SIZE, 0);
                    if (n < 0) {
                        perror("Read error");
                        exit(1);
                    }
                    buff[n] = '\0';
                    printf(CYN "[%d]: %s\n" RESET, cli->connfd, buff);
                    if (strcmp(tmp->password, buff) == 0) {
                        if (tmp->status == 0 || tmp->status == 2) {
                            strcpy(mesg, "account not ready");
                            send(connfd, mesg, strlen(mesg), 0);
                        } 
                        else {
                            tmp->count = 1;
                            strcpy(mesg, "OK");
                            changeClientAccount(tmp->username, connfd);
                            send(connfd, mesg, strlen(mesg), 0);
                            strcpy(online_user, tmp->username);
                            cli->login_status = 1;
                        }
                    }
                    else {
                        if (tmp->count == 3) {
                            updateUserFILE(tmp->username, tmp->password, 0); // 0 : blocked
                            strcpy(mesg, "Account is blocked");
                            printf("%s\n", mesg);
                            send(connfd, mesg, strlen(mesg), 0);
                        }
                        else {
                            strcpy(mesg, "Not OK");
                            tmp->count++;
                            send(connfd, mesg, strlen(mesg), 0);
                        }
                    }
                    break;
                }
                tmp = tmp->next;  
            }
            if (tmp == NULL) {
                strcpy(mesg, "Wrong account! Please register first.");
                send(connfd, mesg, strlen(mesg), 0);
                continue;
            }
        }
        else if (cli->login_status == 1) {
            // receive new password
            if (strcmp(buff, "bye") == 0) {
                strcpy(mesg, "Goodbye ");
                strcat(mesg, online_user);
                changeClientAccount("", connfd);
                cli->login_status = 0;
                strcpy(online_user, "");
                send(connfd, mesg, strlen(mesg), 0);
                continue;
            }
            else if (strcmp(buff, "1") == 0) {
                strcpy(mesg, "Enter new password");
                send(connfd, mesg, strlen(mesg), 0);
                if ((n = recv(connfd, buff, BUFF_SIZE, 0)) < 0) {
                    perror("Read error");
                    exit(1);
                }
                buff[n] = '\0';
                printf(CYN "[%d]: %s\n" RESET, cli->connfd, buff);
                int alphabet = 0, number = 0, i; 
                char alpha[BUFF_SIZE] = {0}, digit[BUFF_SIZE] = {0}; 
                for (i=0; buff[i]!= '\0'; i++) { 
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
                        send(connfd, mesg, strlen(mesg), 0);
                        break;
                    }
                }
                if (alphabet != 0 || number != 0) {
                    strcpy(mesg, digit);
                    strcat(mesg, alpha);
                    send(connfd, mesg, strlen(mesg), 0);
                    updateUserFILE(online_user, buff, 3); // 3 ~ keep the current status
                    send_to_others(buff, connfd);
                }
            }
            else if (strcmp(buff, "2") == 0) {
                strcpy(mesg, "You choose mode online");
                send(connfd, mesg, strlen(mesg), 0);
            }
            else if (strcmp(buff, "3") == 0) {
                // strcpy(mesg, "You choose mode offline");
                srand(time(0));
                // printf(RED "red\n"     RESET);
                // printf(GRN "green\n"   RESET);
                // printf(YEL "yellow\n"  RESET);
                // printf(BLU "blue\n"    RESET);
                // printf(MAG "magenta\n" RESET);
                // printf(CYN "cyan\n"    RESET);
                // printf(WHT "white\n"   RESET);
                int i;
                for (i = 0; i < 15; i++) {
                    char tmp[BUFF_SIZE] = {0};
                    int j = (rand() % (9 - 0 + 1)) + 0;
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
                    if (send(connfd, mesg, strlen(mesg), 0) == -1) {
                        // printf("Host disconnected: socket_fd(%d)\n", client_socket[i]);
                        break;
                    }
                    if ((n = recv(connfd, buff, BUFF_SIZE, 0) > 0)) {
                        buff[n] = '\0';
                        printf(CYN "[%d]: %s\n" RESET, connfd, buff);
                        if (atoi(buff) == 5) {

                        }
                        else if (atoi(buff) != questionBank[i].questions[j].true) {
                            sprintf(mesg, "Sai! Đáp án đúng là %d\n", questionBank[i].questions[j].true);
                            printf("%s\n", mesg);
                            send(connfd, mesg, strlen(mesg), 0);
                            break;
                        }
                    } 
                    else if (n < 0) {
                        perror("Read error");
                        exit(1);
                    }
                }
                if (i == 15) {
                    strcpy(mesg, "\nChúc mừng bạn đã trả lời đúng 15 câu hỏi!\n\n");
                    send(connfd, mesg, strlen(mesg), 0);
                }
            }
        }
    }
    if (n < 0) {
        perror("Read error");
        exit(1);
    }
    return 0;
}

void send_to_others(char *mesg, int curr){
    char online[BUFF_SIZE];
	pthread_mutex_lock(&mutex);
    struct client *tmp = client1;
    while (tmp != NULL) {  
        if (tmp->connfd == curr) {
            strcpy(online, tmp->login_account);
            break;
        }
        tmp = tmp->next;
    }

    tmp = client1;
    while (tmp != NULL) {  
        if (tmp->connfd != curr) {
            if (strcmp(tmp->login_account, online) == 0) {
                send(tmp->connfd, mesg, strlen(mesg), 0);
            }
        }
        tmp = tmp->next;
    }
	pthread_mutex_unlock(&mutex);
}

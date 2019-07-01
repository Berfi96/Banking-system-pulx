#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#define MAX_MSG_LEN 257
#define RESPONSE_BYTES 512
#define REQUEST_BYTES 512

char* receiveMsgFromServer(int sockFD) {
	int numPacketsToReceive = 0;
	int n = read(sockFD, &numPacketsToReceive, sizeof(int));
	if (n <= 0) {
		shutdown(sockFD, SHUT_WR);
		return NULL;
	}
	char *str = (char*) malloc(numPacketsToReceive * RESPONSE_BYTES);
	memset(str, 0, numPacketsToReceive * RESPONSE_BYTES);
	char *str_p = str;
	int i;
	for (i = 0; i < numPacketsToReceive; ++i) {
		int n = read(sockFD, str, RESPONSE_BYTES);
		str = str + RESPONSE_BYTES;
	}
	return str_p;
}

void sendMsgToServer(int sockFD, char *str) {
	int numPacketsToSend = (strlen(str) - 1) / REQUEST_BYTES + 1;
	int n = write(sockFD, &numPacketsToSend, sizeof(int));
	char *msgToSend = (char*) malloc(numPacketsToSend * REQUEST_BYTES);
	strcpy(msgToSend, str);
	int i;
	for (i = 0; i < numPacketsToSend; ++i) {
		int n = write(sockFD, msgToSend, REQUEST_BYTES);
		msgToSend += REQUEST_BYTES;
	}
}

int main(int argc, char **argv) {
	int sockFD, *portNO;
	struct sockaddr_in serv_addr;
	char *msgFromServer;
	char msgToServer[MAX_MSG_LEN], input[35],ip_addr[35];
	printf("\033[H\033[J");	//czyszczenie konsoli
	printf("Witaj w programie klienckim obsługi banku\n");

	printf("Podaj numer portu do połączenia: ");
	   scanf ("%d", &portNO);

	sockFD = socket(AF_INET, SOCK_STREAM, 0);
	//otwarcie socketa

	memset(&serv_addr, 0, sizeof(serv_addr));
	printf("Podaj adres IP serwera do połączenia: ");
	scanf ("%s", ip_addr);

	printf("\n");

	//setting sockaddr_in serv_addr
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portNO);
	inet_aton(ip_addr, &serv_addr.sin_addr);
	connect(sockFD, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
//
//	printf ("\033[H\033[J");	//czyszczenie konsoli
//	printf("Połączenie zostało zestawione.\n");
//	printf("Możesz zalogować się używająć swojego loginu i hasła.\n");
	while (1) {

		msgFromServer = receiveMsgFromServer(sockFD);
		if (msgFromServer == NULL)
			break;

		if (strncmp(msgFromServer, "unauth", 6) == 0) {
			printf("Nieprawidłowa nazwa użytkownika.\n");
			shutdown(sockFD, SHUT_WR);
			break;
		}

	printf(msgFromServer);
	printf("\n");
	free(msgFromServer);
	memset(msgToServer, 0, sizeof(msgToServer));
	scanf("%s", msgToServer);
	sendMsgToServer(sockFD, msgToServer);
	if (strncmp(msgToServer, "exit", 4) == 0) {
		shutdown(sockFD, SHUT_WR);
		break;
	}
	}

while (1) {
	msgFromServer = receiveMsgFromServer(sockFD);
	if (msgFromServer == NULL){
		//printf("Nie udało połączyć się z serwerem.\n");
		break;
	}
	printf(msgFromServer);
	printf("\n");
	free(msgFromServer);
	}
shutdown(sockFD, SHUT_RD);
printf("Kończenie połączenia.\n");
return 0;
}

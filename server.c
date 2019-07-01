#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>

#define MAX_LEN 256
#define MAX_LINES_IN_MS 5

#define CREDIT 10
#define DEBIT 11

#define USER 0
#define ADMIN 2
#define UNAUTH_USER -1

#define RESPONSE_BYTES 512
#define REQUEST_BYTES 512

struct userInfo{
	char userId[255];
	char pass[255];
};

void msgToClient(int clientFD, char *str) {
	int numPacketsToSend = (strlen(str)-1)/RESPONSE_BYTES + 1;
	int n = write(clientFD, &numPacketsToSend, sizeof(int));
	char *msgToSend = (char*)malloc(numPacketsToSend*RESPONSE_BYTES);
	strcpy(msgToSend, str);
	int i;
	for(i = 0; i < numPacketsToSend; ++i) {
		int n = write(clientFD, msgToSend, RESPONSE_BYTES);
		msgToSend += RESPONSE_BYTES;
	}
}

char* receiveMsgFromClient(int clientFD) {
	int numPacketsToReceive = 0;
	int n = read(clientFD, &numPacketsToReceive, sizeof(int));
	if(n <= 0) {
		shutdown(clientFD, SHUT_WR);
		return NULL;
	}
	
	char *str = (char*)malloc(numPacketsToReceive*REQUEST_BYTES);
	memset(str, 0, numPacketsToReceive*REQUEST_BYTES);
	char *str_p = str;
	int i;
	for(i = 0; i < numPacketsToReceive; ++i) {
		int n = read(clientFD, str, REQUEST_BYTES);
		str = str+REQUEST_BYTES;
	}
	return str_p;
}

struct userInfo getUserInfo(int clientFD) {
	int n;
	char *username = "\033[H\033[JPołączenie zostało zestawione.\nMożesz zalogować się używająć swojego loginu i hasła.\nLogin: ";
	char *password = "Hasło: ";
	char *buffU;
	char *buffP;

	//wysłanie zapytania o login

	msgToClient(clientFD,username);
	buffU = receiveMsgFromClient(clientFD);

	//wysłanie zapytania o haslo
	msgToClient(clientFD, password);
	buffP = receiveMsgFromClient(clientFD);

	struct userInfo uInfo;
	memset(&uInfo, 0, sizeof(uInfo));

	//skopiowanie loginu i hasla do UserInfo

	int i;
	for(i = 0; i < 255; ++i) {
		if(buffU[i] != '\n' && buffU[i] != '\0') {
			uInfo.userId[i] = buffU[i];
		} else {
			break;
		}
	}
	uInfo.userId[i] = '\0';

	for(i = 0; i < 255; ++i) {
		if(buffP[i] != '\n' && buffP[i] != '\0') {
			uInfo.pass[i] = buffP[i];
		} else {
			break;
		}
	}
	uInfo.pass[i] = '\0';
	if(buffU != NULL)
		free(buffU);
	buffU = NULL;
	if(buffP != NULL)
		free(buffP);
	buffP = NULL;
	return uInfo;
}

char* readFromFile(FILE *fp) {
	fseek(fp, 0, SEEK_END);	
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if(sz == 0)
		return NULL;
	char *str = (char *)malloc((sz+1)*sizeof(char));
	fread(str, sizeof(char), sz, fp);
	str[sz] = 0;
	return str;
}

int autoryzacja(struct userInfo uInfo) {
	printf("Autoryzowanie użytkownika: \n");
	printf(uInfo.userId);
	printf("\n");
	printf(uInfo.pass);
	printf("\n");
	FILE *fp = fopen("login_file", "r");
	char delim[] = ", \n"; //separator
	char *str = readFromFile(fp);
	fclose(fp);
	char *save_ptr;
	char *tok = strtok_r(str, delim, &save_ptr); //wydzielanie słów z łańcucha
	do {
		if(strcmp(uInfo.userId, tok) == 0) {//znalezienie loginu
			tok = NULL;
			tok = strtok_r(NULL, delim, &save_ptr);
			if(strcmp(uInfo.pass, tok) == 0) {//znalezienie hasla
				tok = NULL;
				tok = strtok_r(NULL, delim, &save_ptr);
				if(strcmp(tok, "U") == 0)
					return USER;	//Sprawdzenie typu użytkownika, jeśli U to zwykły user
				else if(strcmp(tok, "A") == 0)
					return ADMIN; //Sprawdzenie typu użytkownika, jeśli A to admin
				}
		} else {
			tok = strtok_r(NULL, delim, &save_ptr);
			tok = strtok_r(NULL, delim, &save_ptr);
		}
		tok = NULL;
	} while((tok = strtok_r(NULL, delim, &save_ptr)) != NULL);
	if(str!=NULL)
		free(str);
	return UNAUTH_USER;
}

void addStrings(char** str1, const char* str2,char del)
{
    size_t len1 = *str1 ? strlen(*str1) : 0;
    size_t len2 = str2 ? strlen(str2) : 0;
    char *res = realloc(*str1, len1 + len2 + 2);
    if (res)
    {
        res[len1] = del;
        memcpy(res + len1 + 1, str2, len2);
        res[len1 + 1 + len2] = 0;
        *str1 = res;
    }
}

void historiaTransakcji(int clientFD, char *fileName) {
	fileExist(fileName);
	FILE *fp=fopen(fileName, "r");
	char delim[] = "\n";
	char *str = readFromFile(fp);
	fclose(fp);

	char *historiaTrans = NULL;
	char *tok = strtok(str, delim);
	int cnt = 0;
	do {
		if(cnt == 0 && tok != NULL) {
			historiaTrans = (char*)malloc(((strlen(tok)+1))*sizeof(char));
			strcpy(historiaTrans, tok);
			historiaTrans[strlen(tok)] = 0;
		}
		else
			addStrings(&historiaTrans, tok, '\n');
		tok = NULL;
		cnt++;
	} while((tok = strtok(NULL, delim)) != NULL && cnt < MAX_LINES_IN_MS);
	if(str!=NULL)
		free(str);
	if(historiaTrans == NULL)
		msgToClient(clientFD, "None");
	else
		msgToClient(clientFD, historiaTrans);
	if(historiaTrans != NULL)
		free(historiaTrans);
	historiaTrans = NULL;
	str = NULL;
}

char* returnBalance(char *fileName) {
	fileExist(fileName);
	FILE *fp=fopen(fileName, "r");

	char delim[] = ",\n";
	char *str = readFromFile(fp);
	fclose(fp);

	char *save_ptr;

	char *bal = (char*)malloc(2*sizeof(char));
	bal[0] = '0';
	bal[1] = 0;
	char *tok = strtok_r(str, delim, &save_ptr);
	int cnt = 0;
	do {
		if(cnt == 2) {
			bal = (char*)malloc(((strlen(tok)+1))*sizeof(char));
			strcpy(bal, tok);
			bal[strlen(tok)] = 0;
		}
		tok = NULL;
		cnt++;
	} while((tok = strtok_r(NULL, delim, &save_ptr)) != NULL && cnt < 3);
	if(str!=NULL)
		free(str);
	str = NULL;
	return bal;
}


void processUserRequests(int clientFD, struct userInfo uInfo) {
	int n;
	char *buff = NULL;
	char *bal = returnBalance(uInfo.userId);
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//char* startInfo= "Stan Twojego konta wynosi: ";
//addStrings(startInfo,bal,' ');   							tutaj poprawic łączenie stringow

//addStrings(&startInfo,"\nWprowadź \"h\" w celu sprawdzenia historii transakcji lub \"s\" w celu sprawdzenia dostępnych środków.\nMożesz także wyświetlić pomoc wpisując \"help\": "," ");
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	msgToClient(clientFD, "Wprowadź \"h\" w celu sprawdzenia historii transakcji lub \"s\" w celu sprawdzenia dostępnych środków.\nMożesz także wyświetlić pomoc wpisując \"help\": ");
	while(1) {
		if(buff != NULL)
			free(buff);
		buff = receiveMsgFromClient(clientFD);
		if(strcmp(buff, "h") == 0) {
			historiaTransakcji(clientFD, uInfo.userId);
		} else if(strcmp(buff, "s") == 0) {
			bal = returnBalance(uInfo.userId);
			msgToClient(clientFD, bal);
			free(bal);
			bal = NULL;
		} else if(strcmp(buff, "exit") == 0) {
			break;
		}
		if(strcmp(buff, "help") == 0) {
			msgToClient(clientFD, "\nMożesz sprawdzić historie swoich transakcji wpisując \"h\"\nMożesz sprawdzić stan swojego konta wpisując \"s\".");
		}else {
			msgToClient(clientFD, "Nieprawidłowy znak.");
		}
	}
	if(buff != NULL)
		free(buff);
	buff = NULL;
}

int userExists(char *userName) {
	FILE *fp = fopen("login_file", "r");
	char delim[] = ", \n";
	char *str = readFromFile(fp);
	fclose(fp);

	char *save_ptr;
	char *tok = strtok_r(str, delim, &save_ptr);
	do {
		int check = 0;
		if(tok!=NULL && strcmp(tok, userName) == 0) {
			check = 1;
		}
		tok = strtok_r(NULL, delim, &save_ptr);
		tok = strtok_r(NULL, delim, &save_ptr);
		if(strcmp(tok, "U") == USER && check == 1)
			return 1;
		tok = NULL;
	} while((tok = strtok_r(NULL, delim, &save_ptr)) != NULL);
	if(str!=NULL)
		free(str);
	return 0;
}

int getUserName(int clientFD, char **userName) {
	int n;
	char *buff=NULL;
	int toRet = -1;

	msgToClient(clientFD, "Wprowadź nazwę użytkownika na którym chcesz dokonać modyfikacji salda: ");
	while(1) {
		if(buff != NULL)
			free(buff);
		buff = receiveMsgFromClient(clientFD);
		if(strcmp(buff, "exit") == 0) {
			toRet = -1;
			break;
		} else if(userExists(buff)) {
			*userName = (char*)malloc((n+1)*sizeof(char));
			strcpy(*userName, buff);
			toRet = 1;
			break;
		} else {
			msgToClient(clientFD, "Użytkownik nieznany.");
		}
	}
	if(buff != NULL)
		free(buff);
	buff = NULL;
	return toRet;
}

int getQuery(int clientFD) {
	int n;
	char *buff=NULL;
	int toRet = -1;
	msgToClient(clientFD, "Wprowadź znak \"+\" żeby zwiększyć ilość środków na koncie lub znak \"-\" żeby zmniejszyć ");
	while(1) {
		if(buff != NULL)
			free(buff);
		buff = receiveMsgFromClient(clientFD);
		if(strcmp(buff, "exit") == 0) {
			toRet = -1;
			break;
		} else if(strcmp(buff, "+") == 0) {
			toRet = CREDIT;
			break;
		} else if(strcmp(buff, "-") == 0) {
			toRet = DEBIT;
			break;
		} else {
			msgToClient(clientFD, "Wprowadzono niepoprawny znak.\nWprowadź znak \"+\" żeby zwiększyć ilość środków na koncie lub znak \"-\" żeby zmniejszyć ");
		}
	}
	if(buff!=NULL)
		free(buff);
	buff=NULL;
	return toRet;
}

int isANumber(char *num) {
	int i = 0;
	int check = 0;
	for(i = 0; i < strlen(num); ++i) {
		if(isdigit(num[i])) {
			continue;
		}
		else if(num[i] == '.' && check == 0) {
			check = 1;
		}
		else {
			return 0;
		}
	}
	return 1;
}

double getAmount(int clientFD) {
	int n;
	double toRet = -1;
	char *buff=NULL;
	msgToClient(clientFD, "Wprowadź wartość: ");
	while(1) {
		if(buff!=NULL)
			free(buff);
		buff = receiveMsgFromClient(clientFD);
		if(strcmp(buff, "exit") == 0) {
			toRet = -1.0;
			break;
		} else if(isANumber(buff)) {
			toRet = strtod(buff, NULL); //konwertuje łańcuch znaków na liczbę zmiennoprzecinkową
			if(toRet < 0.0f)
				msgToClient(clientFD, "Proszę wprowadzić wartość większą od 0zł.");
			break;
		} else {
			msgToClient(clientFD, "Niepoprawna wartość.");
		}
	}
	if(buff!=NULL)
		free(buff);
	buff=NULL;
	return toRet;
}
void fileExist(char *fileName){
FILE *fp;
	if ((fp = fopen(fileName, "r"))==NULL)
	{
		fp = fopen(fileName, "at");
		fp = fopen(fileName, "r");
		time_t ltime;
		    ltime=time(NULL);

			char *data = (char*)malloc((1 + strlen(asctime(localtime(&ltime))) + 1000 ));
			sprintf(data, "%.*s,%f\n", (int)strlen(asctime(localtime(&ltime)))-1, asctime(localtime(&ltime)), 0);

			fp = fopen(fileName, "w");
			fwrite(data, sizeof(char), strlen(data), fp);
	}
}
void updateUserTransFile(char *fileName, int toCorD, double amount, double curBal) {
	fileExist(fileName);
	FILE *fp=fopen(fileName, "r");

	char *str = readFromFile(fp);
	fclose(fp);
	if(str == NULL) {
		str = "";
	}
	char c_d;
	if(toCorD == CREDIT) {
		curBal += amount;
		c_d = 'C';
	}
	else if(toCorD == DEBIT) {
		curBal -= amount;
		c_d = 'D';
	}

	time_t ltime;
    ltime=time(NULL);

	char *data = (char*)malloc((1 + strlen(asctime(localtime(&ltime))) + 1000 + strlen(str))*sizeof(char));
	sprintf(data, "%.*s,%c,%f\n%s", (int)strlen(asctime(localtime(&ltime)))-1, asctime(localtime(&ltime)), c_d, curBal, str);

	fp = fopen(fileName, "w");
	fwrite(data, sizeof(char), strlen(data), fp);
	fclose(fp);
}

int showInSuffBal(int clientFD) {
	int n;
	int toRet = -1;
	char *buff=NULL;
	msgToClient(clientFD, "Za mała ilość środków na koncie. Czy chcesz kontynuować?[Y/N]");
	while(1) {
		if(buff!=NULL)
			free(buff);
		buff = receiveMsgFromClient(clientFD);
		if(strcmp(buff, "N") == 0) {
			toRet -1;
			break;
		} else if(strcmp(buff, "Y") == 0) {
			toRet = 1;
			break;
		} else {
			msgToClient(clientFD, "Wprowadzono niepoprany znak. Wprowadź [Y/N]");
		}
	}
	if(buff!=NULL)
		free(buff);
	buff=NULL;
	return toRet;
}

void processAdminRequests(int clientFD) {
	while(1) {
		char *userName;
		int ret = getUserName(clientFD, &userName);
		if(ret < 0)
			return;
		ret = getQuery(clientFD);
		if(ret < 0)
			return;
		int toCorD = ret;
		double amount  = getAmount(clientFD);
		if(amount < 0.0)
			return;
		char *bal = returnBalance(userName);
		double curBal = strtod(bal, NULL); //konwertuje łańcuch znaków na liczbę zmiennoprzecinkową
		free(bal);
		bal = NULL;

		if(curBal < amount && toCorD == DEBIT) {
			ret = showInSuffBal(clientFD);
			if(ret < 0)
				return;
			else
				continue;
		}
		updateUserTransFile(userName, toCorD, amount, curBal);
	}
}



void talkToClient(int clientFD) {
	struct userInfo uInfo = getUserInfo(clientFD);
	int uType = autoryzacja(uInfo);
	if(uType == UNAUTH_USER) {
			printf("Nieznany użytkownik.\n");
		} else if(uType == USER) {
			processUserRequests(clientFD, uInfo);
		} else if(uType == ADMIN) {
			processAdminRequests(clientFD);
		}
}

int main(int argc, char **argv) {
	int sockFD, clientFD, portNO, cliSz;
	struct sockaddr_in serv_addr, cli_addr;
	printf("\033[H\033[J");	//czyszczenie konsoli
	printf("Witaj w programie serwerowym obsługi banku\n");
	printf("Podaj numer portu na którym będzie pracował system: ");
    scanf ("%d", &portNO);

	//utworzenie socketu
	sockFD = socket(AF_INET, SOCK_STREAM, 0);

	//initializing variables
	memset((void*)&serv_addr, 0, sizeof(serv_addr));


	//ustawienia serwera
	serv_addr.sin_family = AF_INET;				//ustawienie domeny
	serv_addr.sin_addr.s_addr = INADDR_ANY;		//dopuszczenie kazdego IP
	serv_addr.sin_port = htons(portNO);			//ustawienie nr portu

	bind(sockFD, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	listen(sockFD, 7);
	cliSz = sizeof(cli_addr);

	while(1) {
		memset(&cli_addr, 0, sizeof(cli_addr));
		clientFD = accept(sockFD, (struct sockaddr*)&cli_addr, &cliSz);

		switch(fork())
		{
			case -1:
				printf("Problem z tworzeniem nowego procesu.\n");
				break;
			case 0: {
				close(sockFD);
				talkToClient(clientFD);
				//msgToClient(clientFD,"Zamykanie sesji.");
				exit(EXIT_SUCCESS);
				break;
			}
			default:
				close(clientFD);
				break;
		}
	}

	return 0;
}

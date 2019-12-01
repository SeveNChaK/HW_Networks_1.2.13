#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <regex.h>
#include <dirent.h>
#include <sys/stat.h>

#include "declaration.h"
#include "dexchange.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct Command {
	int argc;
	char argv[QUANTITY_ARGS][SIZE_ARG];
	char sourceCmdLine[SIZE_DATA];
};

struct Client {
	pthread_t threadId;
	int socket;
	char address[DEFAULT_STR];
	int port;
	int number;
} *clients;
int clientQuantity = 0;

struct Theme {
	int id;
	char title[SIZE_DATA];
} *themes;
int currentIdTheme = 0;

struct News {
	int id;
	int idTheme;
	char title[SIZE_DATA];
} *listNews;
int currentIdNews = 0;

void initServerSocket(int *serverSocket, int port);
void* listenerConnetions(void* args);
void kickAllClients();
void kickClient(int kickNum);
void* clientHandler(void* args);
int execClientCommand(const struct Client *client, char *cmdLine, char *errorString);
int execServerCommand(char *cmdLine, char *errorString);
int parseCmd(char *cmdLine, struct Command *cmd, char *errorString);
int validateCommand(struct Command cmd, char *errorString);
int sendListThemes(const struct Client *client, char *errorString);
int sendListNews(const struct Client *client, const struct Command *cmd, char *errorString);
int sendDescriptionNews(const struct Client *client, const int idNews, char *errorString);
int addNews(const struct Client *client, const int idTheme, char *errorString);
int addNewTheme(char *errorString);
void printListClients();
void printHelp();
int checkRegEx(const char *str, const char *mask);
int findNews(const struct Client *client, const char *mask, char *errorString);

int main( int argc, char** argv) {

	if(argc != 2){
		fprintf(stderr, "%s\n%s\n", "Неверное количество аргументов!", "Необходим вызов: ./server [PORT]");
		exit(1);
	}

	int port = *((int*) argv[1]);
	int serverSocket = -1;
	initServerSocket(&serverSocket, port);

	system("rm -R StorageNews");
	system("mkdir StorageNews");
    
	//Создание потока, который будет принимать входящие запросы на соединение
	pthread_t listenerThread;
    if (pthread_create(&listenerThread, NULL, listenerConnetions, (void*) &serverSocket)){
        fprintf(stderr, "%s\n", "Не удалось создать поток прослушивания подключений!");
        exit(1);
    }

    char buf[DEFAULT_STR];
    char errorString[DEFAULT_STR];
	for(;;) {
		bzero(buf, DEFAULT_STR);
		fgets(buf, DEFAULT_STR, stdin);
		buf[strlen(buf) - 1] = '\0';
		int res = execServerCommand(buf, errorString);
		if (res == 0) {
			shutdown(serverSocket, 2);
			close(serverSocket);
			break;
		}
	}

	pthread_join(listenerThread, NULL);
	fprintf(stdout, "%s\n", "Сервер завершил работу.");

	free(themes);
	free(listNews);
	
	return 0;
}

void initServerSocket(int *serverSocket, int port){
    struct sockaddr_in listenerInfo;
	listenerInfo.sin_family = AF_INET;
	listenerInfo.sin_port = htons(port);
	listenerInfo.sin_addr.s_addr = htonl(INADDR_ANY);
	
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket < 0) {
		fprintf(stderr, "%s\n", "Не удалось создать сокет!");
		exit(1);
	}

	int enable = 1;
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
    	fprintf(stderr, "%s\n", "setsockopt(SO_REUSEADDR) failed!");
	}

	int resBind = bind(*serverSocket, (struct sockaddr *)&listenerInfo, sizeof(listenerInfo));
	if (resBind < 0 ) {
		fprintf(stderr, "%s\n", "Не удалось выполнить присваивание имени сокету!");
		exit(1);
	}
		
	if (listen(*serverSocket, 2)) {
		fprintf(stderr, "%s\n", "Не удалось выполнить listen!");
		exit(1);
	}

	fprintf(stdout, "%s\n", "Инициализация серверного сокета прошла успешно.");
}

void* listenerConnetions(void* args){
    int serverSocket = *((int*) args);
    
    int clientSocket;
    struct sockaddr_in inetInfoAboutClient;
	int alen = sizeof(inetInfoAboutClient);
	fprintf(stdout, "%s\n", "Ожидание подключений запущено.");
    for(;;){

        clientSocket = accept(serverSocket, &inetInfoAboutClient, &alen);
        if (clientSocket <= 0){
        	fprintf(stderr, "%s\n", "Ожидание подключений прервано! Возможно сервер остановил свою работу.");
      		break;
        }

        pthread_mutex_lock(&mutex);
       
        int indexClient = clientQuantity;
        clients = (struct Client*) realloc(clients, sizeof(struct Client) * (clientQuantity + 1));

        clients[indexClient].socket = clientSocket;
        strcpy(clients[indexClient].address, inet_ntoa(inetInfoAboutClient.sin_addr));
        clients[indexClient].port = inetInfoAboutClient.sin_port; //TODO возможно тут надо htons или что-то такое
        clients[indexClient].number = indexClient;

        if(pthread_create(&(clients[indexClient].threadId), NULL, clientHandler, (void*) &indexClient)) {
        	clients[indexClient].socket = -1; //Помечаем клиента, которого не удалось обработать как 'мертового'
            fprintf(stderr, "%s\n", "Не удалось создать поток для клиента, клиент не будет обрабатываться!");
            continue;
        }
        
        clientQuantity++;
        pthread_mutex_unlock(&mutex);
    }

    pthread_mutex_lock(&mutex);
    for (int i = 0; i < clientQuantity; i++){
    	pthread_join(clients[i].threadId, NULL);
    }
    pthread_mutex_unlock(&mutex);

    fprintf(stdout, "%s\n", "Ожидание подключений завершено.");
    free(clients);
}

void kickAllClients(){
	pthread_mutex_lock(&mutex);
	int count = clientQuantity;
    pthread_mutex_unlock(&mutex);
    for(int i = 0; i < count; i++){
    	kickClient(i);
    }
    fprintf(stdout, "%s\n", "Отключение всех клиентов завершено.");
}

void kickClient(int kickNum){
	if (kickNum > clientQuantity || clientQuantity == 0) {
		fprintf(stderr, "Клиента №%d не существует.\n", kickNum);
		return;
	}
	pthread_mutex_lock(&mutex);
	shutdown(clients[kickNum].socket, 2);
	close(clients[kickNum].socket);
	clients[kickNum].socket = -1;
    pthread_mutex_unlock(&mutex);
    fprintf(stdout, "Кажись клиент №%d был отключен.\n", kickNum);
}

void* clientHandler(void* args){
	int indexClient = *((int*)args);
	pthread_mutex_lock(&mutex);
	struct Client *client = &(clients[indexClient]);
	int clientSocket = client->socket;
	pthread_mutex_unlock(&mutex);

	fprintf(stdout, "Соединение с клиентом №%d установлено.\n", indexClient);

	struct Package package;
	char errorString[SIZE_DATA];
	for(;;) {
		bzero(errorString, sizeof(errorString));
		if(readPack(clientSocket, &package) < 0){
			kickClient(indexClient);
			break;
		} else {
			if(package.code == CODE_CMD){
				int err = execClientCommand(client, package.data, errorString);
				if(err == -1){
					sendPack(clientSocket, CODE_ERROR, strlen(errorString), errorString);
				}
			} else {
				fprintf(stderr, "Ожидался пакет с кодом - %d, получили пакет с кодом - %d\n", CODE_CMD, package.code);
				sprintf(errorString, "%s\n", "Ожидалась команда! Используйте /help");
				sendPack(clientSocket, CODE_ERROR, strlen(errorString) + 1, errorString);
			}
			sendPack(clientSocket, CODE_OK, strlen("Команда обработана."), "Команда обработана.");
		}
	}

	fprintf(stdout, "Клиент №%d завершил своб работу.\n", indexClient);
}

int parseCmd(char *cmdLine, struct Command *cmd, char *errorString){
	bzero(errorString, sizeof(errorString));
	int countArg = 0;
	char *sep = " ";
	char *arg = strtok(cmdLine, sep);
	fprintf(stdout, "Сари - %s\n", arg);
	if(arg == NULL){
		sprintf(errorString, "Команда: %s - не поддается парсингу.\nВведите корректную команду. Используйте: /help\n", cmdLine);
		return -1;
	}
	while(arg != NULL && countArg <= QUANTITY_ARGS){
		countArg ++;
		if(countArg > QUANTITY_ARGS){
			sprintf(errorString, "Слишком много аргументов в команде: %s - таких команд у нас нет. Используйте: /help\n", cmdLine);
			return -1;
		}
		strcpy(cmd->argv[countArg - 1], arg);
		if (!strcmp(cmd->argv[0], "/find") && countArg == 2) {
			break;
		}
		arg = strtok(NULL, sep);
	}
	cmd->argc = countArg;
	strcpy(cmd->sourceCmdLine, cmdLine);
	return 1;
}

int validateCommand(struct Command cmd, char *errorString){
	bzero(errorString, sizeof(errorString));
	int argc = cmd.argc;
	char *firstArg = cmd.argv[0];
	char *cmdLine = cmd.sourceCmdLine;
	if(!strcmp(firstArg, "/lst")){
		if (argc != 1) {
			sprintf(errorString, "Команда: %s - имеет лишние аргументы. Воспользуейтесь командой: /help\n", cmdLine);
			return -1;
		}
	} else if(!strcmp(firstArg, "/lsn")){
		if (argc > 2) {
			sprintf(errorString, "Команда: %s - имеет много аргументов. Воспользуейтесь командой: /help\n", cmdLine);
			return -1;
		}
		if (argc == 2) {
			int err = checkRegEx(cmd.argv[1], "^[0-9]+$");
			if (err == -1) {
    			sprintf(errorString, "С командой: %s - что-то не так. Воспользуейтесь командой: /help\n", cmdLine);
    			return -1;
			} else if (err == -2) {
				sprintf(errorString, "Команда: %s - имеет некорретные аргументы. Воспользуейтесь командой: /help\n", cmdLine);
				return -1;
			}
			pthread_mutex_lock(&mutex);
			if (atoi(cmd.argv[1]) >= currentIdTheme || currentIdTheme == 0) {
				sprintf(errorString, "Темы №%d - не существует!\n", atoi(cmd.argv[1]));
				pthread_mutex_unlock(&mutex);
				return -1;
			}
			pthread_mutex_unlock(&mutex);
		}
	} else if(!strcmp(firstArg, "/news")){
		if(argc != 2){
			sprintf(errorString, "Команда: %s - имеет много или мало аргументов. Воспользуейтесь командой: /help\n", cmdLine);
			return -1;
		}
		int err = checkRegEx(cmd.argv[1], "^[0-9]+$");
		if (err == -1) {
    		sprintf(errorString, "С командой: %s - что-то не так. Воспользуейтесь командой: /help\n", cmdLine);
    		return -1;
		} else if (err == -2) {
			sprintf(errorString, "Команда: %s - имеет некорретные аргументы. Воспользуейтесь командой: /help\n", cmdLine);
			return -1;
		}
		pthread_mutex_lock(&mutex);
		if (atoi(cmd.argv[1]) >= currentIdNews || currentIdNews == 0) {
			sprintf(errorString, "Новости №%d - не существует!\n", atoi(cmd.argv[1]));
			pthread_mutex_unlock(&mutex);
			return -1;
		}
		pthread_mutex_unlock(&mutex);
	} else if(!strcmp(firstArg, "/addn")){
		if(argc != 2){
			sprintf(errorString, "Команда: %s - имеет много или мало аргументов. Воспользуейтесь командой: /help\n", cmdLine);
			return -1;
		}
		int err = checkRegEx(cmd.argv[1], "^[0-9]+$");
		if (err == -1) {
    		sprintf(errorString, "С командой: %s - что-то не так. Воспользуейтесь командой: /help\n", cmdLine);
    		return -1;
		} else if (err == -2) {
			sprintf(errorString, "Команда: %s - имеет некорретные аргументы. Воспользуейтесь командой: /help\n", cmdLine);
			return -1;
		}
    	pthread_mutex_lock(&mutex);
		if (atoi(cmd.argv[1]) >= currentIdTheme || currentIdTheme == 0) {
			sprintf(errorString, "Темы №%d - не существует!\n", atoi(cmd.argv[1]));
			pthread_mutex_unlock(&mutex);
			return -1;
		}
		pthread_mutex_unlock(&mutex);
	} else if(!strcmp(firstArg, "/kick")){
		if(argc != 2){
			sprintf(errorString, "Команда: %s - имеет много или мало аргументов. Воспользуейтесь командой: /help\n", cmdLine);
			return -1;
		}
		int err = checkRegEx(cmd.argv[1], "^[0-9]+$");
		if (err == -1) {
    		sprintf(errorString, "С командой: %s - что-то не так. Воспользуейтесь командой: /help\n", cmdLine);
    		return -1;
		} else if (err == -2) {
			sprintf(errorString, "Команда: %s - имеет некорретные аргументы. Воспользуейтесь командой: /help\n", cmdLine);
			return -1;
		}
	} else if(!strcmp(firstArg, "/shutdown")){
		if(argc != 1){
			sprintf(errorString, "Команда: %s - имеет лишние аргументы! Воспользуейтесь командой: /help\n", cmdLine);
			return -1;
		}
	} else if(!strcmp(firstArg, "/help")){
		if(argc != 1){
			sprintf(errorString, "Команда: %s - имеет лишние аргументы! Воспользуейтесь командой: /help\n", cmdLine);
			return -1;
		}
	} else if(!strcmp(firstArg, "/lc")){
		if(argc != 1){
			sprintf(errorString, "Команда: %s - имеет лишние аргументы! Воспользуейтесь командой: /help\n", cmdLine);
			return -1;
		}
	} else if(!strcmp(firstArg, "/addt")){
		if(argc != 1){
			sprintf(errorString, "Команда: %s - имеет лишние аргументы! Воспользуейтесь командой: /help\n", cmdLine);
			return -1;
		}
	} else if(!strcmp(firstArg, "/find")){
		if(argc != 2){
			sprintf(errorString, "Команда: %s - имеет мало аргументов! Воспользуейтесь командой: /help\n", cmdLine);
			return -1;
		}
	} else {
		sprintf(errorString, "Неизвстная команда: %s. Воспользуейтесь командой: /help\n", cmdLine);
		return -1;
	}
	return 1;
}

int execClientCommand(const struct Client *client, char *cmdLine, char *errorString){
	bzero(errorString, sizeof(errorString));
	struct Command cmd;
	if(parseCmd(cmdLine, &cmd, errorString) == -1){
		fprintf(stderr, "Не удалось распарсить команду клиента: %s\nОписание ошибки: %s\n", cmdLine, errorString);
		return -1;
	}
	if(validateCommand(cmd, errorString) == -1){
		fprintf(stderr, "Команда клиента: %s - неккоретна!\nОписание ошибки: %s\n", cmdLine, errorString);
		return -1;
	}
	fprintf(stdout, "Команда клиента: %s - корректна.\n", cmdLine);
	if(!strcmp(cmd.argv[0], "/lst")) {
		return sendListThemes(client, errorString);
	} else if(!strcmp(cmd.argv[0], "/lsn")) {
		return sendListNews(client, &cmd, errorString);
	} else if(!strcmp(cmd.argv[0], "/news")) {
		return sendDescriptionNews(client, atoi(cmd.argv[1]), errorString);
	} else if(!strcmp(cmd.argv[0], "/addn")){
		return addNews(client, atoi(cmd.argv[1]), errorString);
	} else if(!strcmp(cmd.argv[0], "/find")){
		return findNews(client, cmd.argv[1], errorString);
	} else {
		fprintf(stderr, "Хоть мы все и проверили, но что-то с ней не так: %s\n", cmdLine);
		return -1;
	}
	return 1;
}

int sendListThemes(const struct Client *client, char *errorString) {
	bzero(errorString, sizeof(errorString));
	pthread_mutex_lock(&mutex);
	int countThemes = currentIdTheme - 1;
	pthread_mutex_unlock(&mutex);
	int err;
	char answer[SIZE_DATA] = { 0 };
	sprintf(answer, "Темы:\n");
	err = sendPack(client->socket, CODE_INFO, strlen(answer), answer);
	if (err == -1) {
		sprintf(errorString, "что-то не так");
		fprintf(stdout, "%s\n", "Не могу отправить список тем!");
		return -1;
	}
	for (int i = 0; i <= countThemes; i++) {
		bzero(answer, sizeof(answer));
		sprintf(answer, "\t%d : %s\n", themes[i].id, themes[i].title);
		err = sendPack(client->socket, CODE_INFO, strlen(answer), answer);
		if (err == -1) {
			sprintf(errorString, "что-то не так");
			fprintf(stdout, "%s\n", "Не могу отправить список тем!");
			return -1;
		}
	}
	return 1;
}

int sendListNews(const struct Client *client, const struct Command *cmd, char *errorString) {
	bzero(errorString, sizeof(errorString));
	pthread_mutex_lock(&mutex);
	int countNews = currentIdNews - 1;
	pthread_mutex_unlock(&mutex);

	int err;
	char answer[SIZE_DATA] = { 0 };

	if (cmd->argc == 2) {
		sprintf(answer, "Новости на тему \'%s\':\n", themes[atoi(cmd->argv[1])].title);
	} else {
		sprintf(answer, "Все новости:\n");
	}
	err = sendPack(client->socket, CODE_INFO, strlen(answer), answer);
	if (err == -1) {
		sprintf(errorString, "что-то не так");
		fprintf(stdout, "%s\n", "Не могу отправить список новостей!");
		return -1;
	}

	for (int i = 0; i <= countNews; i++) {
		bzero(answer, sizeof(answer));
		if (cmd->argc == 2 && listNews[i].idTheme != atoi(cmd->argv[1])) {
			continue;
		}

		if (cmd->argc == 2) {
			sprintf(answer, "\t%d : %s\n", listNews[i].id, listNews[i].title);
		} else {
			sprintf(answer, "\t%d. Тема - %s\n\t%s\n", listNews[i].id, themes[listNews[i].idTheme].title, listNews[i].title);
		}

		err = sendPack(client->socket, CODE_INFO, strlen(answer), answer);
		if (err == -1) {
			sprintf(errorString, "что-то не так");
			fprintf(stdout, "%s\n", "Не могу отправить список новостей!");
			return -1;
		}
	}
	return 1;
}

int sendDescriptionNews(const struct Client *client, const int idNews, char *errorString) {

	char fileName[DEFAULT_STR] = { 0 };
	sprintf(fileName, "StorageNews/%d", idNews);
	FILE *file = fopen(fileName, "r");
	if(file == NULL){
		fprintf(stderr, "Не удалось открыть файл: %s - для чтения!\n", fileName);
		sprintf(errorString, "Не удается открыть новость! Попробуйте позже.");
		return -1;
	}

	char section[SIZE_DATA] = { 0 };
	int err;
	sprintf(section, "Новость №%d. %sТема - \'%s\'\n", idNews, listNews[idNews].title, themes[listNews[idNews].idTheme].title);
	err = sendPack(client->socket, CODE_INFO, sizeof(section), section);
	if(err == -1){
		fprintf(stderr, "Не удалось отправить кусок новости - %s\n", fileName);
		sprintf(errorString, "Не удалось открыть новость. - %s\n", listNews[idNews].title);
		fclose(file);
		return -1;
	}
	while(fgets(section, SIZE_DATA, file) != NULL){
		err = sendPack(client->socket, CODE_INFO, sizeof(section), section);
		if(err == -1){
			fprintf(stderr, "Не удалось отправить кусок новости - %s\n", fileName);
			sprintf(errorString, "Не удалось открыть новость. - %s\n", listNews[idNews].title);
			fclose(file);
			return -1;
		}
		bzero(section, sizeof(section));
	}
	fclose(file);
	return 1;
}

int addNews(const struct Client *client, const int idTheme, char *errorString) {

	pthread_mutex_lock(&mutex);
	int idNews = currentIdNews;
	pthread_mutex_unlock(&mutex);

	char fileName[DEFAULT_STR] = { 0 };
	sprintf(fileName, "StorageNews/%d", idNews);
	FILE *file = fopen(fileName, "w");
	if(file == NULL){
		fprintf(stderr, "Не удалось открыть файл: %s - для записи!\n", fileName);
		sprintf(errorString, "Не удается создать новость! Попробуйте позже.");
		return -1;
	}

	sendPack(client->socket, CODE_ID_THEME_OK, strlen(themes[idTheme].title), themes[idTheme].title);

	struct Package package;
	struct News news;
	news.id = idNews;
	int err;
	for(;;){
		err = readPack(client->socket, &package);
		if(err == -1){
			fprintf(stderr, "Не удалось принять кусок новости - %s. Новость не была создана.\n", fileName);
			fclose(file);
			remove(fileName);
			return -1;
		}

		if(package.code == CODE_TITLE){
			fprintf(stdout, "Получил заголовок - %s\n", package.data);
			strcpy(news.title, package.data);
		} else if(package.code == CODE_DESCRIPTION){
			fprintf(stdout, "Получил описание - %s\n", package.data);
			fputs(package.data, file);
		} else if(package.code == CODE_ERROR){
			fprintf(stderr, "Создание новости \'%s\' прервано. Новость не была сохранена.\n", fileName);
			fclose(file);
			remove(fileName);
			return -1;
		} else if(package.code == CODE_OK){
			fprintf(stdout, "Получил всю новость.");
			break;
		} else {
			fprintf(stderr, "Пришел неправильный пакет с кодом - %d.\n", package.code);
			sprintf(errorString, "Не удалось создать новость.");
			fclose(file);
			remove(fileName);
			return -1;
		}
	}
	fclose(file);

	pthread_mutex_lock(&mutex);
	listNews = (struct News*) realloc(listNews, sizeof(struct News) * (currentIdNews + 1));
	listNews[currentIdNews].id = news.id;
	listNews[currentIdNews].idTheme = idTheme;
	strcpy(listNews[currentIdNews].title, news.title);
	currentIdNews++;
	pthread_mutex_unlock(&mutex);

	sendPack(client->socket, CODE_INFO, strlen("Новость создана.\n") + 1, "Новость создана.\n");

	return 1;
}

int execServerCommand(char *cmdLine, char *errorString){
	bzero(errorString, sizeof(errorString));
	struct Command cmd;
	if(parseCmd(cmdLine, &cmd, errorString) == -1){
		fprintf(stderr, "Не удалось распарсить команду сервера: %s\nОписание ошибки: %s\n", cmdLine, errorString);
		return -1;
	}
	if(validateCommand(cmd, errorString) == -1){
		fprintf(stderr, "Команда сервера: %s - неккоретна!\nОписание ошибки: %s\n", cmdLine, errorString);
		return -1;
	}
	fprintf(stdout, "Команда сервера: %s - корректна.\n", cmd.argv[0]);
	if(!strcmp(cmd.argv[0], "/kick")){
		kickClient(atoi(cmd.argv[1]));
	} else if(!strcmp(cmd.argv[0], "/shutdown")){
		kickAllClients();
		return 0;
	} else if(!strcmp(cmd.argv[0], "/help")){
		printHelp();
	} else if(!strcmp(cmd.argv[0], "/lc")){
		printListClients();
	} else if(!strcmp(cmd.argv[0], "/addt")){
		return addNewTheme(errorString);
	} else {
		sprintf(errorString, "Хоть мы все ипроверили, но что-то с командой не так: %s\n", cmdLine);
		return -1;
	}
	return 1;
}

int addNewTheme(char *errorString) {
	bzero(errorString, sizeof(errorString));
	
	fprintf(stdout, "Введите название темы (не более %d символов): ", DEFAULT_STR);
	
	char buf[DEFAULT_STR] = { 0 };
	fgets(buf, DEFAULT_STR, stdin);
	buf[strlen(buf) - 1] = '\0';
	
	pthread_mutex_lock(&mutex);
	themes = (struct Theme*) realloc(themes, sizeof(struct Theme) * (currentIdTheme + 1));
	themes[currentIdTheme].id = currentIdTheme;
	strcpy(themes[currentIdTheme].title, buf);
	int temp = currentIdTheme;
	currentIdTheme++;
	pthread_mutex_unlock(&mutex);

	fprintf(stdout, "Тема №%d : %s - успешно добавлена.\n", temp, buf);

	return 1;
}

void printListClients() {
	pthread_mutex_lock(&mutex);
	int res = 0;
	fprintf(stdout, "Клиенты:\n");
	for (int i = 0; i < clientQuantity; i++) {
		if (clients[i].socket != -1) {
			fprintf(stdout, "\t№:%d - IP:%s - PORT: %d\n", clients[i].number, clients[i].address, clients[i].port);
			res = 1;
		}
	}
	if (res == 0) {
		fprintf(stdout, "\tотсутствуют =(\n");
	}
	pthread_mutex_unlock(&mutex);
}

void printHelp() {
	fprintf(stdout, "\nДоступные команды:\n");
	fprintf(stdout, "\t/lс - список клиентов\n");
	fprintf(stdout, "\t/addt - добавление темы\n");
	fprintf(stdout, "\t/kick [NUMBER_CLIENT] - отключение клиента\n");
	fprintf(stdout, "\t/shutdown - выключение сервера\n");
	fprintf(stdout, "\n");
}

int checkRegEx(const char *str, const char *mask) {
	regex_t preg;
    int err = regcomp (&preg, mask, REG_EXTENDED);
    if(err != 0){
    	fprintf(stderr, "Не получилось скомпилировать регулярное выражение: %s\n", mask);
    	return -1;
    }
    regmatch_t pm;
    err = regexec (&preg, str, 0, &pm, 0);
    if(err != 0){
    	return -2;
    }
    return 0;
}

int findNews(const struct Client *client, const char *mask, char *errorString) {
	pthread_mutex_lock(&mutex);
	int countNews = currentIdNews - 1;
	pthread_mutex_unlock(&mutex);

	char info[SIZE_DATA] = { 0 };
	int err;
	sprintf(info, "Результаты поиска:\n");
	err = sendPack(client->socket, CODE_INFO, sizeof(info), info);
	if(err == -1){
		fprintf(stderr, "Не удалось отправить результаты поиска до конца!\n");
		sprintf(errorString, "Не удалось отправить результаты поиска до конца!\n");
		return -1;
	}
	for (int i = 0; i <= countNews; i++) {
		bzero(info, sizeof(info));
		if (checkRegEx(listNews[i].title, mask) == 0) {
			sprintf(info, "\t%d. Тема - %s\n\t%s\n", listNews[i].id, themes[listNews[i].idTheme].title, listNews[i].title);
			err = sendPack(client->socket, CODE_INFO, sizeof(info), info);
			if(err == -1){
				fprintf(stderr, "Не удалось отправить результаты поиска до конца!\n");
				sprintf(errorString, "Не удалось отправить результаты поиска до конца!\n");
				return -1;
			}
		}
	}
	return 1;
}
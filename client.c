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
#include <sys/stat.h>

#include "declaration.h"
#include "dexchange.h"

int main(int argc, char** argv) {

	if(argc != 3){
		fprintf(stderr, "%s\n%s\n", "Неверное количество аргументов!", "Необходим вызов: ./client [IP] [PORT]");
		exit(1);
	}
	char *ip = (char *) argv[1];
	int port = *((int*) argv[2]);

	struct sockaddr_in peer;
	peer.sin_family = AF_INET;
	peer.sin_addr.s_addr = inet_addr(ip); 
	peer.sin_port = htons(port);

	int sock = -1, rc = -1;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		exit(1);
	}
	rc = connect(sock, (struct sockaddr*) &peer, sizeof(peer));
	if(rc == -1){
		fprintf(stderr, "Проблемы с подключением по IP - %s и PORT - %d\n", ip, port);
		exit(1);
	}

	char inputBuf[SIZE_DATA];
	for(;;){
		bzero(inputBuf, sizeof(inputBuf));
		fgets(inputBuf, sizeof(inputBuf), stdin);
		inputBuf[strlen(inputBuf) - 1] = '\0';

		if (!strcmp("/help", inputBuf)) {
			printHelp();
			continue;
		} else if (!strcmp("/disconnect", inputBuf)) {
			close(sock);
			break;
		}

		if (sendPack(sock, CODE_CMD, strlen(inputBuf) + 1, inputBuf) == -1) {
			fprintf(stderr, "Проблемы с отправкой команды на сервер. Соединение разорвано!\n");
			break;
		} else {
			if (execCommand(sock) == 0) {
				break;
			}
		}
	}
	
	return 0;
}

void printHelp() {
	fprintf(stdout, "\nДоступные команды:\n");
	fprintf(stdout, "\t/lst - список доступных тем\n");
	fprintf(stdout, "\t/lsn - список всех новостей\n");
	fprintf(stdout, "\t/lsn [NUMBER_THEME] - список всех новостей по теме\n");
	fprintf(stdout, "\t/addn [NUMBER_THEME] - добавление новости по теме\n");
	fprintf(stdout, "\t\t/s - сохранение новости\n");
	fprintf(stdout, "\t\t/с - отмена создания новости\n");
	fprintf(stdout, "\t/news [NUMBER_NEWS] - просмотр новости\n");
	fprintf(stdout, "\t/find [SEARCH_STR] - поиск нвостей по названию\n");
	fprintf(stdout, "\t/disconnect - выключение клиента\n");
	fprintf(stdout, "\n");
}

int execCommand(int sock){
	struct Package package;
	int code = -1;
	for(;;){
		if (readPack(sock, &package) == -1) {
			fprintf(stderr, "Проблемы с получением ответа от сервера. Соединение разорвано!\n");
			return 0;
		}

		code = package.code;

		if(code == CODE_ERROR){
			fprintf(stderr, "Возникла ошибка!\n%s", package.data);
		} else if(code == CODE_INFO){
			fprintf(stdout, "%s", package.data);
		} else if(code == CODE_ID_THEME_OK){
			createNews(sock, package.data);
		} else if(code == CODE_OK){
			fprintf(stdout, "Все класс! Команда обработана.\n");
			break;
		}
	}
	return 1;
}

void createNews(const int sock, const char *themeTitle) {

	fprintf(stdout, "\nСоздание новости:\n\tТема: %s\nВведите название новости:\n\t", themeTitle);	

	int err;
	char inputBuf[SIZE_DATA];
	bzero(inputBuf, sizeof(inputBuf));
	fgets(inputBuf, sizeof(inputBuf), stdin);

	if (!strcmp("/s\n", inputBuf)) {
		err = sendPack(sock, CODE_OK, 3, "OK");
		if(err == -1){
			fprintf(stderr, "Не удалось отправить подтверждение создания. Новость не будет создана.\n");
			return;
		}
		return;
	} else if (!strcmp("/c\n", inputBuf)) {
		sendPack(sock, CODE_ERROR, sizeof(STR_CANCEL), STR_CANCEL);
		fprintf(stderr, "Новость не будет создана.\n");
		return;
	}

	err = sendPack(sock, CODE_TITLE, strlen(inputBuf) + 1, inputBuf);
	if(err == -1){
		fprintf(stderr, "Не удалось отправить строку. Новость не будет создана.\n");
		return;
	}

	fprintf(stdout, "Введите описание:\n");
	for(;;){
		bzero(inputBuf, sizeof(inputBuf));
		fgets(inputBuf, sizeof(inputBuf), stdin);

		if (!strcmp("/s\n", inputBuf)) {
			err = sendPack(sock, CODE_OK, sizeof(STR_OK), STR_OK);
			if(err == -1){
				fprintf(stderr, "Не удалось отправить подтверждение создания. Новость не будет создана.\n");
				return;
			}
			break;
		} else if (!strcmp("/c\n", inputBuf)) {
			sendPack(sock, CODE_ERROR, sizeof(STR_CANCEL), STR_CANCEL);
			fprintf(stderr, "Новость не будет создана.\n");
			break;
		}

		err = sendPack(sock, CODE_DESCRIPTION, strlen(inputBuf) + 1, inputBuf);
		if(err == -1){
			fprintf(stderr, "Не удалось отправить строку. Новость не будет создана.\n");
			return;
		}
	}
}
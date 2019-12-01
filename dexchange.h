#ifndef DEXCHANGE_H
#define DEXCHANGE_H

int readPack(int socket, struct Package *package){
	bzero(package->data, sizeof(package->data));
	int result = readN(socket, package, sizeof(struct Package));
	if(result < 0 ){
		fprintf(stderr, "%s\n", "Не удалось считать пакет!");
		return -1;
	}
	// fprintf(stdout, "Получен пакет:\n\tкод - %d\n\tданные - %s\n", package->code, package->data);
	return result;
}

int sendPack(int socket, int code, int sizeData, char *data){
	struct Package package;
	bzero(package.data, sizeof(package.data));
	package.code = code;
	package.sizeData = sizeData;
	memcpy(package.data, data, sizeData);
	int res = send(socket, &package, sizeof(struct Package), 0);
	if(res < 0 ){
		fprintf(stderr, "%s\n", "Не удалось отправить пакет!");
		return -1;
	}
	return res;
}

int readN(int socket, char* buf, int length){
	int result = 0;
	int readedBytes = 0;
	int sizeMsg = length;
	while(sizeMsg > 0){
		readedBytes = recv(socket, buf + result, sizeMsg, 0);
		if (readedBytes <= 0){
			return -1;
		}
		result += readedBytes;
		sizeMsg -= readedBytes;
	}
	return result;
}

#endif

#ifndef DECLARATION_H
#define DECLARATION_H

#define SIZE_DATA 1000
#define SIZE_ARG 100
#define QUANTITY_ARGS 2

#define DEFAULT_STR 500

#define CODE_CMD 100
#define CODE_INFO 101
#define CODE_ERROR 102
#define CODE_OK 103
#define CODE_TITLE 104
#define CODE_DESCRIPTION 105
#define CODE_ID_THEME_OK 106

#define STR_OK "OK"
#define STR_CANCEL "CANCEL"

struct Package {
	int code;
	int sizeData;
	char data[SIZE_DATA];
};

#endif

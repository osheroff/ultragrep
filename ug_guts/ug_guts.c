#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include "pcre.h"
#include "req_extract.h"


typedef struct {
    time_t start_time;
    time_t end_time;
    int num_regexps;
    pcre **regexps;

}context_t;


int check_request(int lines, char **request, time_t request_time, pcre **regexps, int num_regexps)
{
	int *matches, i, j, matched;

	matches = malloc(sizeof(int) * num_regexps);
	memset(matches, 0, (sizeof(int) * num_regexps));

	for(i=0; i < lines; i++) {
		for(j=0; j < num_regexps; j++) {
			int ovector[30];
			if ( matches[j] ) continue;

			matched = pcre_exec(regexps[j], NULL, request[i], strlen(request[i]), 0, 0, ovector, 30);
			if ( matched > 0 )
				matches[j] = 1;
		}
	}

	matched = 1;
	for (j=0; j < num_regexps; j++) {
		matched &= matches[j];
	}

	free(matches);
	return(matched);
}


int rails_req_match(char *line, ssize_t line_size, time_t* tv) {
    const char* error;
    int erroffset;
    int ovector[30];
    char *date_buf;
    struct tm request_tm;
    int matched;
    static pcre* regex= NULL;

    if(regex == NULL) {
        regex = pcre_compile("^Processing.*(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2})", 0, &error, &erroffset, NULL);
    }

    matched = pcre_exec(regex, NULL, line, line_size,0,0,ovector, 30);
    if(matched > 0) {
        pcre_get_substring(line, ovector, matched, 1, (const char **)&date_buf);
        strptime(date_buf, "%Y-%m-%d %H:%M:%S", &request_tm);
        *tv = mktime(&request_tm);
        free(date_buf);
    }
    return(matched);
}

void print_rails_request(int request_lines, char **request)
{
	int i, j;
	putchar('\n');

	for(i=0; i < request_lines; i++)
		printf("%s", request[i]);

	for(j=0; j < strlen(request[request_lines - 1]) && j < 80; j++ )
		putchar('-');

	putchar('\n');
	fflush(stdout);
}

int handle_rails_request(request_t* req, context_t* cxt) {
    static int tick = 0;
    if(req->time > cxt->start_time &&
            check_request(req->lines,  req->buf, req->time, cxt->regexps, cxt->num_regexps)) {
					printf("@@%lu\n", req->time);

                                        print_rails_request(req->lines, req->buf);
    }

    if ( tick % 100 == 0 )
        printf("@@%lu\n", req->time);
    tick++;
    if(req->time > cxt->end_time)
        return -1;
    return 0;
}

int work_req_match(char* line, ssize_t line_size, time_t* tv) {
    const char* error;
    int erroffset;
    int ovector[30];
    char *date_buf;
    struct tm request_tm;
    int matched;
    static pcre* regex= NULL;
    static pcre* time_regex = NULL;

    if(regex == NULL) {
        regex = pcre_compile("Starting this session", 0, &error, &erroffset, NULL);
        time_regex = pcre_compile("\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}", 0, &error, &erroffset, NULL);
    }

    matched = pcre_exec(regex, NULL, line, line_size,0,0,ovector, 30);
    if(matched > 0) {
        matched = pcre_exec(time_regex, NULL, line, line_size, 0,0, ovector, 30);
        if(matched > 0) {
            pcre_get_substring(line, ovector, matched, 0, (const char **)&date_buf);
            strptime(date_buf, "%Y-%m-%d %H:%M:%S", &request_tm);
            *tv = mktime(&request_tm);
            free(date_buf);
        }
    }
    return(matched);
}

void print_work_request(int request_lines, char **request) {
    int i, j;
    putchar('\n');

    for(i=0; i < request_lines; i++)
        printf("%d: %s", i, request[i]);

    for(j=0; j < strlen(request[request_lines - 1]) && j < 80; j++ )
        putchar('-');

    putchar('\n');
    fflush(stdout);
}

int handle_work_request(request_t* req, context_t* cxt) {
    static int tick = 0;
    if(req->time > cxt->start_time &&
            check_request(req->lines,  req->buf, req->time, cxt->regexps, cxt->num_regexps)) {
        printf("@@%lu\n", req->time);

        print_work_request(req->lines, req->buf);
    }

    if ( tick % 100 == 0 )
        printf("@@%lu\n", req->time);
    tick++;
    if(req->time > cxt->end_time)
        return -1;
    return 0;

}

int main(int argc, char **argv)
{
    int i;
    context_t *cxt;
    request_t *req;
    const char *error;
    int erroffset;
    char *line = NULL;
    ssize_t line_size, allocated;

    if ( argc < 4 ) {
        fprintf(stderr, "Usage: ug_guts start_time end_time regexps [... regexps]\n");
        exit(1);
    }

    cxt = malloc(sizeof(context_t));
    cxt->start_time = atol(argv[1]);
    cxt->end_time = atol(argv[2]);

    cxt->num_regexps = argc - 3;
    cxt->regexps = malloc(sizeof(pcre *) * cxt->num_regexps);

    for ( i = 3; i < argc; i++) {
        cxt->regexps[i-3] = pcre_compile(argv[i], 0, &error, &erroffset, NULL);
        if ( error ) {
            fprintf(stderr, "Error compiling regexp \"%s\": %s\n", argv[i], error);
            exit;
        }
    }

    req = malloc(sizeof(request_t));

    req_extractor_init(req, &rails_req_match);

    while(1) {
        int ret;
        line_size = getline(&line, &allocated, stdin);
        ret = req_extract_each_line(line, line_size, req, &handle_rails_request, cxt);
        if(ret == EOF_REACHED || ret == STOP_SIGNAL) {
            break;
        }
        line = NULL;
    }
}


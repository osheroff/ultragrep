#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pcre.h"
#include "req_matcher.h"
#include "rails_req.h"
#include "work_req.h"
#include "json_req.h"
#include <unistd.h>


typedef struct {
    time_t start_time;
    time_t end_time;
    int num_regexps;
    pcre **regexps;
    req_matcher_t *m;
} context_t;

static const char* commandparams="l:s:e:k:";
static const char* usage = "Usage: %s ug_guts (work|app|json) start_time end_time regexps [... regexps]\n\n";

int check_request(int lines, char **request, time_t request_time, pcre ** regexps, int num_regexps)
{
    int *matches, i, j, matched;

    matches = malloc(sizeof(int) * num_regexps);
    memset(matches, 0, (sizeof(int) * num_regexps));

    for (i = 0; i < lines; i++) {
        for (j = 0; j < num_regexps; j++) {
            int ovector[30];
            if (matches[j])
                continue;

            matched = pcre_exec(regexps[j], NULL, request[i], strlen(request[i]), 0, 0, ovector, 30);
            if (matched > 0)
                matches[j] = 1;
        }
    }
    matched = 1;
    for (j = 0; j < num_regexps; j++) {
        matched &= matches[j];
    }

    free(matches);
    return (matched);
}

void print_request(int request_lines, char **request)
{
    int i, j;
    putchar('\n');

    for (i = 0; i < request_lines; i++)
        printf("%s", request[i]);

    for (j = 0; j < strlen(request[request_lines - 1]) && j < 80; j++)
        putchar('-');

    putchar('\n');
    fflush(stdout);
}

void handle_request(request_t * req, void *cxt_arg)
{
    static int time = 0;
    context_t *cxt = (context_t *) cxt_arg;
    if ((req->time > cxt->start_time && check_request(req->lines, req->buf, req->time, cxt->regexps, cxt->num_regexps))) {
        if (req->time != 0) {
            printf("@@%lu\n", req->time);
        }

        print_request(req->lines, req->buf);
    }
    if (req->time > time) {
        time = req->time;
        printf("@@%lu\n", time);
    }
    if (req->time > cxt->end_time) {
        cxt->m->stop(cxt->m);
    }
}


int parse_args(int argc,char** argv, context_t *cxt)
{
    extern char *optarg;
    extern int optind;
    const char *error;
    int erroffset;
    int optValue=0, err = 0;
    int lflag=0 , sflag=0, eflag=0, kflag=0;
    int retValue = 1;
    int i;

    //getOpt(): command line parsing
    while ((optValue = getopt(argc, argv, commandparams))!= -1) {
        switch (optValue) {
            case 'l':
                lflag = 1;
                if (strcmp(optarg, "work") == 0) {
                    cxt->m = work_req_matcher(&handle_request, NULL, cxt);
                } else if (strcmp(optarg, "app") == 0) {
                    cxt->m = rails_req_matcher(&handle_request, NULL, cxt);
                } else if (strcmp(optarg, "json") == 0 ){
                    cxt->m = json_req_matcher(&handle_json_request, NULL, cxt);
                } else {
                    fprintf(stderr, "%s",usage);
                    exit(1);
                    }
                break;
            case 's':
                sflag = 1;
                cxt->start_time = atol(optarg);
                break;
            case 'e':
                eflag = 1;
                cxt->end_time = atol(optarg);
                break;
            case 'k':
                kflag = 1;
                add_key_value(optarg, cxt);
                break;
            case '?':
                fprintf(stderr, "? values: %d\n\n", lflag);
                printf("%s",usage);
                err = 1;
                retValue = -1;
                break;
            case -1:    //Options exhausted
                fprintf(stderr, "-1 values: %d\n\n", lflag);
                printf("%s",usage);
                break;
            default:
                abort();
            }
    }
    if (lflag == 0 || sflag == 0 || eflag == 0 ) {	// mandatory fields
        printf(usage, argv[0]);
        return(-1);
    }
    //need at least one argument (change +1 to +2 for two, etc. as needeed)
    else if ((optind + 1 ) > argc) {
        printf(usage, argv[0]);
        return(-1);
        } else if (err) {
            printf( usage, argv[0]);
            return(-1);
        }

    if (optind < argc) {	//these are the arguments after the command-line options
        cxt->num_regexps = argc - optind;
        cxt->regexps = malloc(sizeof(pcre *) * cxt->num_regexps);
        for (i=0; optind < argc; ++optind, i++){
            cxt->regexps[i] = pcre_compile(argv[optind], 0, &error, &erroffset, NULL);
            if (error) {
                printf("Error compiling regexp \"%s\": %s\n", argv[optind], error);
                exit;
            }
         }
    } else {
        printf("no arguments left to process\n");
        return(-1);
    }
    return retValue;
}

int main(int argc, char **argv)
{
    context_t *cxt;
    char *line = NULL;
    ssize_t line_size, allocated;

    if (argc < 5) {
        fprintf(stderr, "\%s", usage);
        exit(1);
    }

    cxt = malloc(sizeof(context_t));
    if (parse_args(argc, argv, cxt) > 0 ) {
        while (1) {
            int ret;
            line_size = getline(&line, &allocated, stdin);
            ret = cxt->m->process_line(cxt->m, line, line_size, 0);
            if (ret == EOF_REACHED || ret == STOP_SIGNAL) {
                break;
            }
            line = NULL;
        }
    }
}

// ex: set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "pcre.h"
#include "rails_req.h"
#include "work_req.h"
#include "ug_index.h"

#define USAGE "Usage: ug_build_index (work|app) file\n"


// index file format
// [64bit,64bit] -- timestamp, file offset 
// [32bit, Nbytes] -- extra data

void handle_request(request_t* req, build_idx_context_t* cxt) {
    time_t floored_time;
    floored_time = req->time - (req->time % INDEX_EVERY);
    if ( !cxt->last_index_time || floored_time > cxt->last_index_time ) {
        ug_write_index(cxt->index, floored_time, req->offset, 
        ug_write_index(cxt->index, floored_time, req->offset, NULL, 0);
        cxt->last_index_time = floored_time;
    }
}

int main(int argc, char **argv)
{
    build_idx_context_t *cxt;
    char *line = NULL, *index_fname = NULL, *dir;
    ssize_t line_size, allocated;


    if ( argc < 3 ) {
        fprintf(stderr, USAGE);
        exit(1);
    }

    cxt = malloc(sizeof(build_idx_context_t));
    memset(cxt, 0, sizeof(build_idx_context_t));

    if(strcmp(argv[1],"work") == 0)
    {
        cxt->m = work_req_matcher(&handle_request, NULL, cxt);
    }
    else if(strcmp(argv[1], "app") == 0)
    {
        cxt->m = rails_req_matcher(&handle_request, NULL, cxt);
    }
    else
    {
        fprintf(stderr, USAGE);
        exit(1);
    }

    cxt->flog = fopen(argv[2], "r");
    if ( !cxt->flog ) { 
      perror("Couldn't open log file");
      exit(1);
    }

    index_fname = ug_get_index_fname(argv[2]);

    cxt->findex = fopen(index_fname, "r+");
    if ( cxt->findex ) { 
        struct ug_index idx;
        ug_get_last_index_entry(cxt->findex, &idx);
        fseeko(cxt->flog, idx.offset, SEEK_SET);
    } else {
        cxt->findex = fopen(index_fname, "w+");
    }

    if ( !cxt->findex ) { 
        perror("Couldn't open index file");
        exit(1);
    }

    if ( strcmp(argv[2] + (strlen(argv[2]) - 3), ".gz") == 0 ) {
        build_gz_index(cxt);
    } else {
        index.data_size = 0;
        index.data = NULL;
        cxt.index = &index;

        while(1) {
            int ret;
            index.offset = ftello(cxt->flog);
            line_size = getline(&line, &allocated, cxt->flog);
            ret = cxt->m->process_line(cxt->m, line, line_size);

            if(ret == EOF_REACHED || ret == STOP_SIGNAL)
                break;
          
            free(line);
            line = NULL;
      }
    }

    fclose(cxt->index);
    fclose(cxt->flog);
}



#include <stdbool.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "metadata.h"
#include "dav1d_cli_parse.h"

static FILE *metadata_file = NULL;
static int frame_count = 0;
static int filename_len = 4096;

static void wr(const char *text)
{
    fprintf(metadata_file, text);
}

static void wr_int(int v)
{
    fprintf(metadata_file, "%d", v);
}

static void wr_float(float f)
{
    fprintf(metadata_file, "%.2f", f);
}

static void write_indent(int indents)
{
    wr("\n");
    while (indents--) {
        wr("\t");
    }
}

void output_frame_metadata(CLISettings *const cli_settings, Dav1dPicture *p)
{
    if (cli_settings->metadatafile == NULL){
        return;
    }

    char filename[4096];
    snprintf(filename, filename_len, "%s/frame_%d.json", cli_settings->metadatafile, frame_count);
    metadata_file = fopen(filename, "w");

    wr("{");
    if (p->frame_hdr){
        write_prop_int("frame_type", p->frame_hdr->frame_type, 1, true);
        write_prop_int("height", p->frame_hdr->height, 1, false);
        write_prop_int("width", p->frame_hdr->width, 1, false);
        write_prop_int("frame_offset", p->frame_hdr->frame_offset, 1, false);
        //write_prop_int("width", p->m., 1, false);
    } else{
        puts("none");
    }

    metadata_end(cli_settings);
    frame_count++;
}

void create_metadata(CLISettings *const cli_settings)
{

    if (cli_settings->metadatafile == NULL){
        return;
    }

    char filename[4096];
    snprintf(filename, filename_len, "%s", cli_settings->metadatafile);

    int tmp_dir = mkdir(filename, S_IRWXU | S_IRWXG);
    if (tmp_dir != 0 && errno != EEXIST) {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
            strcpy(cwd, ".");
        printf("Failed to create %s/tmp directory! Errno: %d\n", cwd, errno);
    }

    frame_count = 0;
}

void metadata_end(CLISettings *const cli_settings)
{

    if (cli_settings->metadatafile == NULL){
        return;
    }

    wr("\n}\n");
    fclose(metadata_file);
}

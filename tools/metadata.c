/*
#include <stdbool.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "metadata.h"


static FILE *metadata_file = fopen("metadata.txt", "w");
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

static void write_prop_int(const char *prop, int val, int indent, bool is_first)
{
    if (!is_first) {
        wr(",");
    }
    write_indent(indent);
    wr("\"");
    wr(prop);
    wr("\":");
    wr_int(val);
}

static void write_prop_float(const char *prop, float val, int indent, bool is_first)
{
    if (!is_first) {
        wr(",");
    }
    write_indent(indent);
    wr("\"");
    wr(prop);
    wr("\":");
    wr_float(val);
}

static void begin_write_array(const char *name, int indent, bool is_first)
{
    if (!is_first) {
        wr(",");
    }
    write_indent(indent);
    wr("\"");
    wr(name);
    wr("\":[");
}

static void end_write_array(int indent)
{
    write_indent(indent);
    wr("]");
}

void metadata_begin()
{
    if (metadata_type == METADATA_NONE) {
        return;
    }

    int tmp_dir = mkdir("./tmp", S_IRWXU | S_IRWXG);
    if (tmp_dir != 0 && errno != EEXIST) {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
            strcpy(cwd, ".");
        printf("Failed to create %s/tmp directory! Errno: %d\n", cwd, errno);
    }

    metadata_file = fopen("./tmp/metadata.json", "w");
    wr("{");
    begin_write_array("frames", 1, true);

    frame_count = 0;
}

static void begin_write_object(int indent, bool is_first)
{
    if (!is_first) {
        wr(",");
    }
    write_indent(indent);
    wr("{");
}

static void end_write_object(int indent)
{
    write_indent(indent);
    wr("}");
}

static void write_macroblock(Macroblock *mb, int is_first, int indent)
{
    begin_write_object(indent, is_first);
    //stuff
    end_write_object(indent);
}

static void write_slice(VideoParameters *vid, int index, int indent)
{
    Slice *slice = vid->ppSliceList[index];

    write_prop_int("qp", slice->qp, indent, true);
    begin_write_array("macroblocks", indent, false);
    for (int i = slice->start_mb_nr; i < slice->end_mb_nr_plus1; i++) {
        write_macroblock(&vid->mb_data[i], i == slice->start_mb_nr, indent+1);
    }
    end_write_array(indent);
}

static void write_frame(VideoParameters *vid, int indent)
{
    begin_write_array("slices", indent, false);
    for (int i = 0; i < vid->iNumOfSlicesDecoded; i++) {
        begin_write_object(indent+1, i == 0);
        write_slice(vid, i, indent+2);
        end_write_object(indent+1);
    }
    end_write_array(indent);
}

void metadata_gather(VideoParameters *vid)
{
    char filename[filename_len];
    snprintf(filename, filename_len, "./tmp/metadata_%d.json", frame_count);
    metadata_file = fopen(filename, "w");
    wr("{");
    write_frame(vid, 1);
    wr("\n}\n");
    fclose(metadata_file);
    frame_count++;
}

void metadata_end()
{
    end_write_array(1);
    wr("\n}\n");
    fclose(metadata_file);
}

*/
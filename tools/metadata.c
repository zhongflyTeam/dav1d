/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
    fprintf(metadata_file, "%s", text);
}

static void wr_int(int v)
{
    fprintf(metadata_file, "%d", v);
}

/*
static void wr_float(float f)
{
    fprintf(metadata_file, "%.2f", f);
}
*/

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

/*
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

*/

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
        write_prop_int("width", p->frame_hdr->width[0], 1, false);
        write_prop_int("frame_offset", p->frame_hdr->frame_offset, 1, false);
        //write_prop_int("width", p->m., 1, false);
    } else{
        puts("none");
    }

    metadata_end(cli_settings);
    frame_count++;
}

/*
static int make_directory(const char* name)
   {
       int res;
   #ifdef __linux__
       res = mkdir(name, 775);
   #else
       res = _mkdir(name);
   #endif
        return res;
   }
*/

void create_metadata(CLISettings *const cli_settings)
{

    if (cli_settings->metadatafile == NULL){
        return;
    }

    char filename[4096];
    snprintf(filename, filename_len, "%s", cli_settings->metadatafile);

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

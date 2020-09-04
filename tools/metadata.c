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

static void end_set(int indent)
{
    write_indent(indent);
    wr("}");
}

static void write_set(const char *prop, int indent, bool is_first)
{
    if (!is_first) {
        wr(",");
    }
    write_indent(indent);
    wr("\"");
    wr(prop);
    wr("\":");
    wr("{");
}

void output_frame_metadata(CLISettings *const cli_settings, Dav1dPicture *p)
{
    if (cli_settings->metadatafile == NULL){
        return;
    }

    char filename[4096];
    snprintf(filename, filename_len, "frame_%d.json", frame_count);
    metadata_file = fopen(filename, "w");

    wr("{");
        write_prop_int("frame_type", p->frame_hdr->frame_type, 1, true);
        write_prop_int("height", p->frame_hdr->height, 1, false);
        write_prop_int("width", p->frame_hdr->width[0], 1, false);
        write_prop_int("frame_offset", p->frame_hdr->frame_offset, 1, false);

        write_set("delta", 1, false);
        write_prop_int("delta_lf_multi", p->frame_hdr->delta.lf.multi, 2, true);
        write_prop_int("delta_lf_present", p->frame_hdr->delta.lf.present, 2, false);
        write_prop_int("delta_lf_res_log2", p->frame_hdr->delta.lf.res_log2, 2, false);
        end_set(1);

        write_prop_int("warp motion", p->frame_hdr->warp_motion, 1, false);
        write_prop_int("use_ref_frame_mvs", p->frame_hdr->use_ref_frame_mvs, 1, false);
        write_prop_int("txfm_mode", p->frame_hdr->txfm_mode, 1, false);
        write_prop_int("temporal_id", p->frame_hdr->temporal_id, 1, false);
        write_prop_int("switchable_motion_mode", p->frame_hdr->switchable_motion_mode, 1, false);
        write_prop_int("switchable_comp_refs", p->frame_hdr->switchable_comp_refs, 1, false);

        write_set("super_res", 1, false);
        write_prop_int("enabled", p->frame_hdr->super_res.enabled, 2, true);
        write_prop_int("width_scale_denominator", p->frame_hdr->super_res.width_scale_denominator, 2, false);
        end_set(1);

        write_prop_int("subpel_filter_mode", p->frame_hdr->subpel_filter_mode, 1, false);
        write_prop_int("spatial_id", p->frame_hdr->spatial_id, 1, false);
        write_prop_int("showable_frame", p->frame_hdr->showable_frame, 1, false);
        write_prop_int("show_frame", p->frame_hdr->show_frame, 1, false);
        write_prop_int("show_existing_frame", p->frame_hdr->show_existing_frame, 1, false);

        write_set("skip_modes", 1, false);
        write_prop_int("skip_mode_refs", p->frame_hdr->skip_mode_refs[0], 2, true);
        write_prop_int("skip_mode_allowed", p->frame_hdr->skip_mode_allowed, 2, false);
        write_prop_int("skip_mode_enabled", p->frame_hdr->skip_mode_enabled, 2, false);
        end_set(1);

        write_set("segmentation", 1, false);
        write_prop_int("enabled", p->frame_hdr->segmentation.enabled, 2, true);
        write_prop_int("lossless", p->frame_hdr->segmentation.lossless[0], 2, false);
        write_prop_int("qidx", p->frame_hdr->segmentation.qidx[0], 2, false);
        write_prop_int("temporal", p->frame_hdr->segmentation.temporal, 2, false);
        write_prop_int("update_data", p->frame_hdr->segmentation.update_data, 2, false);
        write_prop_int("update_map", p->frame_hdr->segmentation.update_map, 2, false);
        end_set(1);

        write_prop_int("render_width", p->frame_hdr->render_width, 1, false);
        write_prop_int("render_height", p->frame_hdr->render_height, 1, false);
        write_prop_int("refresh_frame_flags", p->frame_hdr->refresh_frame_flags, 1, false);
        write_prop_int("refresh_context", p->frame_hdr->refresh_context, 1, false);
        write_prop_int("refidx", p->frame_hdr->refidx[0], 1, false);
        write_prop_int("reduced_txtp_set", p->frame_hdr->reduced_txtp_set, 1, false);

        write_set("quant", 1, false);
        write_prop_int("quant.qm", p->frame_hdr->quant.qm, 2, true);
        write_prop_int("quant.qm_u", p->frame_hdr->quant.qm_u, 2, false);
        write_prop_int("quant.qm_v", p->frame_hdr->quant.qm_v, 2, false);
        write_prop_int("quant.qm_y", p->frame_hdr->quant.qm_y, 2, false);
        write_prop_int("quant.uac_delta", p->frame_hdr->quant.uac_delta, 2, false);
        write_prop_int("quant.udc_delta", p->frame_hdr->quant.udc_delta, 2, false);
        write_prop_int("quant.vac_delta", p->frame_hdr->quant.vac_delta, 2, false);
        write_prop_int("quant.vdc_delta", p->frame_hdr->quant.vdc_delta, 2, false);
        write_prop_int("quant.yac", p->frame_hdr->quant.yac, 2, false);
        write_prop_int("quant.ydc_delta", p->frame_hdr->quant.ydc_delta, 2, false);
        end_set(1);

        write_set("restoration", 1, false);
        write_prop_int("restoration.type", p->frame_hdr->restoration.type[0], 2, true);
        write_prop_int("restoration.unit_size", p->frame_hdr->restoration.unit_size[0], 2, false);
        end_set(1);

        write_set("tiling", 1, false);
        write_prop_int("col_start_sb", p->frame_hdr->tiling.col_start_sb[0], 2, true);
        write_prop_int("cols", p->frame_hdr->tiling.cols, 2, false);
        write_prop_int("log2_cols", p->frame_hdr->tiling.log2_cols, 2, false);
        write_prop_int("log2_rows", p->frame_hdr->tiling.log2_rows, 2, false);
        write_prop_int("max_log2_cols", p->frame_hdr->tiling.max_log2_cols, 2, false);
        write_prop_int("max_log2_rows", p->frame_hdr->tiling.max_log2_rows, 2, false);
        write_prop_int("min_log2_cols", p->frame_hdr->tiling.min_log2_cols, 2, false);
        write_prop_int("min_log2_rows", p->frame_hdr->tiling.min_log2_rows, 2, false);
        write_prop_int("n_bytes", p->frame_hdr->tiling.n_bytes, 2, false);
        write_prop_int("row_start_sb", p->frame_hdr->tiling.row_start_sb[0], 2, false);
        write_prop_int("uniform", p->frame_hdr->tiling.uniform, 2, false);
        write_prop_int("update", p->frame_hdr->tiling.update, 2, false);
        end_set(1);

        write_prop_int("existing_frame_idx", p->frame_hdr->existing_frame_idx, 1, false);
        write_prop_int("error_resilient_mode", p->frame_hdr->error_resilient_mode, 1, false);
        write_prop_int("disable_cdf_update", p->frame_hdr->disable_cdf_update, 1, false);
        write_prop_int("buffer_removal_time_present", p->frame_hdr->buffer_removal_time_present, 1, false);
        write_prop_int("allow_screen_content_tools", p->frame_hdr->allow_screen_content_tools, 1, false);
        write_prop_int("allow_intrabc", p->frame_hdr->allow_intrabc, 1, false);
        write_prop_int("all_lossless", p->frame_hdr->all_lossless, 1, false);
        write_prop_int("frame_size_override", p->frame_hdr->frame_size_override, 1, false);
        write_prop_int("frame_ref_short_signaling", p->frame_hdr->frame_ref_short_signaling, 1, false);
        write_prop_int("frame_presentation_delay", p->frame_hdr->frame_presentation_delay, 1, false);
        write_prop_int("frame_offset", p->frame_hdr->frame_offset, 1, false);
        write_prop_int("frame_id", p->frame_hdr->frame_id, 1, false);
        write_prop_int("force_integer_mv", p->frame_hdr->force_integer_mv, 1, false);
        write_prop_int("hp", p->frame_hdr->hp, 1, false);
        write_prop_int("have_render_size", p->frame_hdr->have_render_size, 1, false);
        write_prop_int("primary_ref_frame", p->frame_hdr->primary_ref_frame, 1, false);

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

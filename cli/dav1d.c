/*
 * ..
 */

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "include/version.h"

#include "input/input.h"

#include "output/output.h"

#include "dav1d_cli_parse.h"

static void print_stats(const int istty, const unsigned n,
                        const unsigned num)
{
    const char *pre_string = istty ? "\r" : "";
    const char *post_string = istty ? "" : "\n";

    if (num == 0xFFFFFFFFU) {
        fprintf(stderr, "%sDecoded %u frames%s", pre_string, n, post_string);
    } else {
        fprintf(stderr, "%sDecoded %u/%u frames (%.1lf%%)%s",
                pre_string, n, num, 100.0 * n / num, post_string);
    }
}

int main(const int argc, char *const *const argv) {
    const int istty = isatty(fileno(stderr));
    int res = 0;
    CLISettings cli_settings;
    Dav1dSettings lib_settings;
    DemuxerContext *in;
    MuxerContext *out = NULL;
    Dav1dPicture p;
    Dav1dContext *c;
    Dav1dData data;
    unsigned n_out = 0, total, fps[2];
    const char *version = dav1d_version();

    if (strcmp(version, DAV1D_VERSION)) {
        fprintf(stderr, "Version mismatch (library: %s, executable: %s)\n",
                version, DAV1D_VERSION);
        return -1;
    }

    dav1d_init();
    init_demuxers();
    init_muxers();
    parse(argc, argv, &cli_settings, &lib_settings);

    if ((res = input_open(&in, cli_settings.inputfile,
                          fps, &total)) < 0)
    {
        return res;
    }
    if ((res = input_read(in, &data)) < 0)
        return res;

    if (!cli_settings.quiet)
        fprintf(stderr, "Dav1d %s - by Two Orioles\n", DAV1D_VERSION);

    //getc(stdin);
    if (cli_settings.limit != 0 && cli_settings.limit < total)
        total = cli_settings.limit;

    if ((res = dav1d_open(&c, &lib_settings)))
        return res;

    do {
        memset(&p, 0, sizeof(p));
        if ((res = dav1d_decode(c, &data, &p)) < 0) {
            if (res != -EAGAIN) {
                fprintf(stderr, "Error decoding frame: %s\n",
                        strerror(-res));
                break;
            }
            res = 0;
        } else {
            if (!n_out) {
                if ((res = output_open(&out, cli_settings.muxer,
                                       cli_settings.outputfile,
                                       &p.p, fps)) < 0)
                {
                    return res;
                }
            }
            if ((res = output_write(out, &p)) < 0)
                break;
            n_out++;
            if (!cli_settings.quiet)
                print_stats(istty, n_out, total);
        }

        if (cli_settings.limit && n_out == cli_settings.limit)
            break;
    } while (!input_read(in, &data));

    // flush
    if (res == 0) while (n_out < cli_settings.limit) {
        if ((res = dav1d_decode(c, NULL, &p)) < 0) {
            if (res == -EAGAIN) res = 0;
            else {
                fprintf(stderr, "Error decoding frame: %s\n",
                        strerror(-res));
            }
            break;
        } else {
            if (!n_out) {
                if ((res = output_open(&out, cli_settings.muxer,
                                       cli_settings.outputfile,
                                       &p.p, fps)) < 0)
                {
                    return res;
                }
            }
            if ((res = output_write(out, &p)) < 0)
                break;
            n_out++;
            if (!cli_settings.quiet)
                print_stats(istty, n_out, total);
        }
    }

    input_close(in);
    if (out) {
        if (!cli_settings.quiet && istty)
            fprintf(stderr, "\n");
        output_close(out);
    } else {
        fprintf(stderr, "No data decoded\n");
        res = 1;
    }
    dav1d_close(c);

    return res;
}


#ifndef DAV1D_METADATA_H
#define DAV1D_METADATA_H

#include "dav1d_cli_parse.h"

void output_frame_metadata(CLISettings *const cli_settings, Dav1dPicture *p);
void metadata_end(CLISettings *const cli_settings);
void create_metadata(CLISettings *const cli_settings);


#endif /* DAV1D_METADATA_H */

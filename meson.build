project('dav1d', ['c'],
    version: '0.0.1',
    default_options: ['c_std=c11'],
    meson_version: '>= 0.47.0')

dav1d_src_root = meson.current_source_dir()
cdata = configuration_data()
cc = meson.get_compiler('c')

dav1d_inc_dirs = []
dav1d_inc_dirs += include_directories('include')

#
# Option handling
#
dav1d_bitdepths = get_option('bitdepths')
foreach bitdepth : dav1d_bitdepths
    cdata.set('CONFIG_@0@BPC'.format(bitdepth), 1)
endforeach

#
# Generate config headers
#
config_h_target = configure_file(output: 'config.h', configuration: cdata)

#
# OS/Compiler feature detection
#
if not cc.check_header('stdatomic.h')
    error('Atomics not supported')
endif

if cc.has_argument('-mpreferred-stack-boundary=5')
    stackalign_flag = '-mpreferred-stack-boundary=5'
    stackrealign_flag = '-mincoming-stack-boundary=4'
elif cc.has_argument('-mstack-alignment=32')
    stackalign_flag = '-mstack-alignment=32'
    stackrealign_flag = '-mstackrealign'
else
    error('Failed to specify stack alignment')
endif

if cc.has_argument('-fvisibility=hidden')
    add_project_arguments('-fvisibility=hidden', language: 'c')
else
    warning('Compiler does not support -fvisibility=hidden, all symbols will be public!')
endif
    

subdir('include')

#
# dav1d library
#
libdav1d_tmpl_sources = files(
    'src/ipred.c',
    'src/itx.c',
    'src/ipred_prepare.c',
    'src/lf_apply.c',
    'src/loopfilter.c',
    'src/mc.c',
    'src/cdef_apply.c',
    'src/cdef.c',
    'src/lr_apply.c',
    'src/looprestoration.c',
    'src/recon.c'
)


# Build a helper library for each bitdepth
bitdepth_libs = []
foreach bitdepth : dav1d_bitdepths
    bitdepth_libs += static_library(
        'dav1d_bitdepth_@0@'.format(bitdepth),
        libdav1d_tmpl_sources, config_h_target,
        c_args: ['-DBITDEPTH=@0@'.format(bitdepth), stackalign_flag],
        install: false,
        build_by_default: false,
    )
endforeach

libdav1d_sources = files(
    'src/picture.c',
    'src/data.c',
    'src/ref.c',
    'src/getbits.c',
    'src/obu.c',
    'src/decode.c',
    'src/cdf.c',
    'src/msac.c',
    'src/tables.c',
    'src/scan.c',
    'src/dequant_tables.c',
    'src/intra_edge.c',
    'src/lf_mask.c',
    'src/ref_mvs.c',
    'src/warpmv.c',
    'src/wedge.c',
    'src/qm.c',
    'src/thread_task.c'
)

libdav1d_tmp = static_library('dav1d_tmp',
    libdav1d_sources,
    link_with: bitdepth_libs,
    include_directories: dav1d_inc_dirs,
    c_args: stackalign_flag
)

libdav1d = library('dav1d',
    'src/lib.c', rev_target,
    version: '0.0.1',
    link_with: libdav1d_tmp,
    include_directories: dav1d_inc_dirs,
    c_args: stackrealign_flag
)

#
# dav1d cli tool
#
dav1d_sources = files(
    'tools/dav1d.c',
    'tools/dav1d_cli_parse.c',
    'tools/input/input.c',
    'tools/input/ivf.c',
    'tools/output/md5.c',
    'tools/output/output.c',
    'tools/output/y4m2.c',
    'tools/output/yuv.c'
)

dav1d = executable('dav1d',
    dav1d_sources, rev_target,
    link_with: libdav1d,
    include_directories: dav1d_inc_dirs + [include_directories('tools')]
)

#
# dav1d revision file generation
#
dav1d_git_dir = join_paths(dav1d_src_root, '.git')
rev_target = vcs_tag(command: [
        'git', '--git-dir', dav1d_git_dir,
        'describe', '--tags', '--long',
        '--match', '?.*.*', '--always'
    ],
    input: 'version.h.in',
    output: 'version.h'
)

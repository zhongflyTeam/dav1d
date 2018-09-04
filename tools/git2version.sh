#!/bin/bash -eu
version=$(git show-ref --head --abbrev=8 | grep " HEAD" | head -n1 | cut -d" " -f1)
tag=$(git show-ref --tags | grep "${version}" | cut -d"/" -f3 | cut -d" " -f2 | tr '_' '.')
[ -z "${tag}" ] && tag="${version}"
echo "/* auto-generated, do not edit */"
echo "#define DAV1D_VERSION \"${tag}\""

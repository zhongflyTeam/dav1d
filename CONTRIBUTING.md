# Contribution guide for dav1d

## CoC
The [VideoLAN Code of Conduct](https://wiki.videolan.org/CoC) applies to this project.

## Codebase language

The codebase is developed with the following assumptions.

For the library:
- C language with C99 version, without the VLA or the Complex (*\_\_STDC_NO_COMPLEX__*) features, and without compiler extensions,
- asm in .asm files, using the NASM syntax,
- no C++ is allowed, whatever the version.


For the tools:
- C *(see above for restrictions)*
- Rust
- C++ only for the MFT.

We will make reasonable efforts for compilers that are a bit older, but we won't support gcc 3 or MSVC 2012.

## Authorship

Please provide a correct authorship for your commit logs.

We will reject anonymous contributions for now. Known pseudonyms from the multimedia community are accepted.

This project is respecting **Copyright** and **Droit d'auteur**. There is no copyright attribution or CLA.

## Commit logs

Please read [How to Write a Git Commit Message](https://chris.beams.io/posts/git-commit/).

## Submit requests (WIP)

- Code
- Test
- Try
- Submit patches

## Patent license

You need to read and understand the [AV1 patents license](doc/PATENTS), before committing.

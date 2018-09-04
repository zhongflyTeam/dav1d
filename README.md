# dav1d

**dav1d** is a new **AV1** cross-platform **d** ecoder, open-source, and focused on speed and correctness.

The canonical repository URL for this repo is https://code.videolan.org/videolan/dav1d

## Goal and Features

The goal of this project is to provide a decoder for most platforms, and achieve the highest speed possible on these platforms, until hardware decoders for AV1 are accessible to the masses.

The goal is to support all features from AV1, including all subsampling and bit-depth.

The project can host simple tools in the future, or simple wrappings *(like an MFT transform, for example)*.

## License

**dav1d** is released under a very liberal license, a contrario from the other VideoLAN projects, so that it can be embedded anywhere, including non-open-source software; or even drivers, for hybrid decoders.

This is the same reasoning as for libvorbis, [RMS on vorbis](https://lwn.net/2001/0301/a/rms-ov-license.php3)

# Roadmap

The plan is the folllowing:

1. Complete C implementation of the decoder,
2. Provide a usable API,
3. Port to most platforms,
4. Make it fast, by writing asm.

We hope to have a completely usable version, with better performance than libaom, by the end of 2018.

# Contribute

Currently, we are looking for help from:
- C developers,
- asm developers,
- platform-specific developers.

Our contributions guideline are very strict, because we want a coherent codebase to simplify maintenance, and achieve the best speed possible.

Notably, the codebase is in pure C and asm.

We are on IRC, on the **#dav1d** channel on *Freenode*.

See the contribution document.

## CLA

There is no CLA.

People will keep their copyright and their authorship rights.

VideoLAN will only have the collective work rights.

## CoC

The usual [VideoLAN Code of Conduct](https://wiki.videolan.org/CoC) applies to this project.

# Compile

1. Install [Meson](https://mesonbuild.com/)
2. Run `meson build`

# Support

This project partially funded by the *Alliance for Open Media*, and is supported by TwoOrioles and VideoLabs.

Those 2 companies can provide support and integration help, if you need it.


# FAQ

## Why don't you develop on libaom?

- We believe that libaom is a very good library, but it was developed for research purposes during AV1 creation.  
But we think that a clean-implementation can get us faster decoding, in the same way that *ffvp9* was faster than *libvpx*.

## Is dav1d a recursive acronym?

- Yes.

## Can I help?

- Yes. See the contributions document.

## I am not a developer. Can I help?

- Yes. We need testers, bug reporters, and documentation writers.

## What about the AV1 patent license?

This project is an implementation of a decoder. It gives you no special rights on the AV1 patents.

Please read the [AV1 patent license](doc/PATENTS) that applies to the AV1 specification and codec.

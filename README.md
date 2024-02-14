# rude

RUDE is a RUdimentary DEduplicating user-space filesystem, mainly designed as an exercise.


## Goal

This is a FUSE-based filesystem that transparently auto-deduplicates
files as soon as they are made read-only, and re-duplicates them as
soon as they are made writable again.

It is intended to save space on repeated software installs of similar
versions.

Install the development fuse3 libraries:

        sudo apt install fuse3 libfuse3-dev

# rude

RUDE is a RUdimentary DEduplicating user-space filesystem, mainly designed as an exercise.


## Goal

This is a FUSE-based filesystem that transparently auto-deduplicates
files as soon as they are made read-only for the owner (S_IWUSR, +200),
and re-duplicates them as soon as they are made writable again.

It is intended to save space on repeated, common installs of software
of similar versions.

It is designed to have minimum overhead on all operations, except `chmod u+w` and `u-w`.

## prerequisites
Install the development fuse3 libraries:

        sudo apt install fuse3 libfuse3-dev

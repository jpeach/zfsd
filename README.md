# zfsd - ZFS Daemon

zfsd is a daemon that manages ZFS storage pools and exposes them
via API plugins.

## Installation

Building `zfsd` requires the autoconf, automake, libtool, GCC or Clang,
libacl, libaio, OpenSSL, zlib and ltdl packages. On Fedora-derived
systems, the [install-builddeps](scripts/install-builddeps) script will
install the required packages.

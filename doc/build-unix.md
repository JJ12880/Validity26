UNIX BUILD NOTES
====================
Some notes on how to build Validity in Unix.

(For BSD specific instructions, see `build-*bsd.md` in this directory.)

To Build
---------------------

```bash
./autogen.sh
./configure
make # use "-j N" for N parallel jobs
make install # optional
```

See below for instructions on how to [install the dependencies on popular Linux
distributions](#linux-distribution-specific-instructions), or the
[dependencies](#dependencies) section for a complete overview.

If you have an existing `wallet.dat`, read [Berkeley DB](#berkeley-db) before you
build: a build using your distribution's Berkeley DB can't open it.

## Memory Requirements

C++ compilers are memory-hungry. It is recommended to have at least 1.5 GB of
memory available when compiling Validity. On systems with less, gcc can be
tuned to conserve memory with additional CXXFLAGS:


    ./configure CXXFLAGS="--param ggc-min-expand=1 --param ggc-min-heapsize=32768"

Alternatively, or in addition, debugging information can be skipped for compilation. The default compile flags are
`-g -O2`, and can be changed with:

    ./configure CXXFLAGS="-O2"

Finally, clang (often less resource hungry) can be used instead of gcc, which is used by default:

    ./configure CXX=clang++ CC=clang

Running a node needs more than building one. The block index alone takes about 1.8 GB at the
current chain height, and it keeps growing. 4 GB of RAM is recommended for a full sync.

## Linux Distribution Specific Instructions

### Tested distributions

Each of these built `validityd` and `validity-qt` from source with the distribution's own
packages, started both on a regtest datadir, and created descriptor and legacy wallets.
Tests ran in x86_64 containers on 2026-10-30, using the package lists below. The GUI ran under
Xvfb, except on AlmaLinux 10, which has no Xvfb, where it ran on Qt's offscreen platform.

| Distribution | GCC | Boost | Qt | Distro BDB | External signer |
| --- | --- | --- | --- | --- | --- |
| Ubuntu 20.04 | 9.4 | 1.71 | 5.12.8 | 5.3 | off |
| Ubuntu 22.04 | 11.4 | 1.74 | 5.15.3 | 5.3 | off |
| Ubuntu 24.04 | 13.3 | 1.83 | 5.15.13 | 5.3 | on |
| Ubuntu 26.04 | 15.2 | 1.90 | 5.15.18 | 5.3 | off |
| Linux Mint 22 | 13.3 | 1.83 | 5.15.13 | 5.3 | on |
| Debian 11 | 10.2 | 1.74 | 5.15.2 | 5.3 | off |
| Debian 12 | 12.2 | 1.74 | 5.15.8 | 5.3 | off |
| Debian 13 | 14.2 | 1.83 | 5.15.15 | 5.3 | on |
| Fedora 43 | 15.3 | 1.83 | 5.15.18 | 5.3 | on |
| Fedora 44 | 16.2 | 1.90 | 5.15.18 | 5.3 | off |
| AlmaLinux 8 | 13.3 (gcc-toolset-13) | 1.66 | 5.15.3 | 5.3 | off |
| AlmaLinux 9 | 11.5 | 1.75 | 5.15.9 | 5.3 | off |
| AlmaLinux 10 | 14.3 | 1.83 | 5.15.18 | 5.3 | on |
| Arch Linux | 16.2 | 1.92 | 5.15.19 | 5.3 | off |
| openSUSE Leap 15.6 | 13.3 (gcc13) | 1.75 | 5.15.12 | 4.8 | off |
| openSUSE Leap 16.0 | 15.3 | 1.86 | 5.15.17 | 4.8 | on |
| openSUSE Tumbleweed | 16.2 | 1.92 | 5.15.19 | 4.8 | off |

"External signer" is hardware signer support. `configure` turns it off automatically when
the installed Boost.Process doesn't provide what the code needs.

Not tested: macOS, the BSDs and non-x86_64 hardware. Rocky Linux and RHEL should follow the
AlmaLinux instructions, but only AlmaLinux was tested.

Minimal container images lack tools that desktop systems already have. Add `findutils`,
`diffutils`, `which`, `gawk`, `tar` and `gzip` if they're missing. Without `find`, libtool
silently builds an incomplete libsecp256k1, and the link then fails with
`undefined reference to secp256k1_pre_g`.

### Ubuntu & Debian

#### Dependency Build Instructions

Build requirements:

    sudo apt-get install build-essential libtool autotools-dev automake pkg-config bsdmainutils python3

Now, you can either build from self-compiled [depends](#dependencies) or install the required dependencies:

    sudo apt-get install libevent-dev libboost-dev

SQLite is required for the descriptor wallet:

    sudo apt install libsqlite3-dev

Berkeley DB is only required for the legacy wallet, which includes every wallet made by 13.x. To open existing
wallets, build Berkeley DB 6.2 [yourself](#berkeley-db). Ubuntu and Debian only package Berkeley DB 5.3, which can't
read them:

    sudo apt install libdb5.3++-dev

If you don't need existing wallets, pass `--with-incompatible-bdb` to configure to use that package.

To build Validity without wallet, see [*Disable-wallet mode*](#disable-wallet-mode)

Optional port mapping libraries (see: `--with-miniupnpc` and `--with-natpmp`):

    sudo apt install libminiupnpc-dev libnatpmp-dev

ZMQ dependencies (provides ZMQ API):

    sudo apt-get install libzmq3-dev

User-Space, Statically Defined Tracing (USDT) dependencies:

    sudo apt install systemtap-sdt-dev

GUI dependencies:

If you want to build validity-qt, make sure that the required packages for Qt development
are installed. Qt 5 is necessary to build the GUI.
To build without GUI pass `--without-gui`.

To build with Qt 5 you need the following:

    sudo apt-get install qtbase5-dev qttools5-dev qttools5-dev-tools

Additionally, to support Wayland protocol for modern desktop environments:

    sudo apt install qtwayland5

libqrencode (optional) can be installed with:

    sudo apt-get install libqrencode-dev

Once these are installed, they will be found by configure and a validity-qt executable will be
built by default.

Linux Mint uses the same packages. Debian 11 is past its support period, so its security packages have moved:
point `/etc/apt/sources.list` at `http://archive.debian.org/debian bullseye main` and
`http://archive.debian.org/debian-security bullseye-security main`, and run `apt-get` with
`-o Acquire::Check-Valid-Until=false`.

### Fedora

#### Dependency Build Instructions

Build requirements:

    sudo dnf install gcc-c++ libtool make autoconf automake python3

Now, you can either build from self-compiled [depends](#dependencies) or install the required dependencies:

    sudo dnf install libevent-devel boost-devel

SQLite is required for the descriptor wallet:

    sudo dnf install sqlite-devel

Berkeley DB is only required for the legacy wallet, which includes every wallet made by 13.x. To open existing
wallets, build Berkeley DB 6.2 [yourself](#berkeley-db). Fedora only packages Berkeley DB 5.3, which can't read them:

    sudo dnf install libdb-cxx-devel

If you don't need existing wallets, pass `--with-incompatible-bdb` to configure to use that package.

To build Validity without wallet, see [*Disable-wallet mode*](#disable-wallet-mode)

Optional port mapping libraries (see: `--with-miniupnpc` and `--with-natpmp`):

    sudo dnf install miniupnpc-devel libnatpmp-devel

ZMQ dependencies (provides ZMQ API):

    sudo dnf install zeromq-devel

User-Space, Statically Defined Tracing (USDT) dependencies:

    sudo dnf install systemtap-sdt-devel

GUI dependencies:

If you want to build validity-qt, make sure that the required packages for Qt development
are installed. Qt 5 is necessary to build the GUI.
To build without GUI pass `--without-gui`.

To build with Qt 5 you need the following:

    sudo dnf install qt5-qttools-devel qt5-qtbase-devel

Additionally, to support Wayland protocol for modern desktop environments:

    sudo dnf install qt5-qtwayland

libqrencode (optional) can be installed with:

    sudo dnf install qrencode-devel

Once these are installed, they will be found by configure and a validity-qt executable will be
built by default.

### AlmaLinux, Rocky Linux & RHEL

#### Dependency Build Instructions

Enable the extra repositories. On version 8 the repository is called `powertools`; on 9 and 10 it's `crb`:

    sudo dnf install 'dnf-command(config-manager)' epel-release
    sudo dnf config-manager --set-enabled crb

Version 9 and 10 packages:

    sudo dnf install gcc-c++ make libtool autoconf automake python3 libevent-devel boost-devel sqlite-devel qt5-qttools-devel qt5-qtbase-devel

Version 8's default GCC 8 is too old, so use the GCC 13 toolset, and enable it in the shell you build from:

    sudo dnf install gcc-toolset-13-gcc-c++ make libtool autoconf automake python3 libevent-devel boost-devel sqlite-devel qt5-qttools-devel qt5-qtbase-devel
    source /opt/rh/gcc-toolset-13/enable

Optional packages on all three:

    sudo dnf install libdb-cxx-devel miniupnpc-devel libnatpmp-devel zeromq-devel qrencode-devel

`libdb-cxx-devel` is Berkeley DB 5.3, which needs `--with-incompatible-bdb` and can't open existing wallets. See
[Berkeley DB](#berkeley-db). On version 10, Qt 5 and Berkeley DB come from EPEL.

### Arch Linux

#### Dependency Build Instructions

    sudo pacman -Syu --needed base-devel boost libevent sqlite qt5-base qt5-tools python

Optional packages:

    sudo pacman -S --needed db5.3 miniupnpc libnatpmp zeromq qrencode

`db5.3` is Berkeley DB 5.3, which needs `--with-incompatible-bdb` and can't open existing wallets. See
[Berkeley DB](#berkeley-db).

### openSUSE

#### Dependency Build Instructions

Tumbleweed and Leap 16:

    sudo zypper install gcc-c++ libtool make autoconf automake python3 pkg-config libevent-devel libboost_headers-devel sqlite3-devel libqt5-qtbase-devel libqt5-qttools-devel

Leap 15.6 needs the newer compiler and Boost packages:

    sudo zypper install gcc13-c++ libtool make autoconf automake python3 pkg-config libevent-devel libboost_headers1_75_0-devel sqlite3-devel libqt5-qtbase-devel libqt5-qttools-devel
    ./configure CC=gcc-13 CXX=g++-13

If Leap 15.6 reports a conflict between `libqt5-qttools-devel` and `libncurses6`, as it did when tested, add
`--force-resolution` to the install. This downgrades `libncurses6`.

Optional packages:

    sudo zypper install libdb-4_8-devel libminiupnpc-devel cppzmq-devel qrencode-devel

`libnatpmp-devel` isn't available on any of these, and `cppzmq-devel` isn't available on Leap 16. `libdb-4_8-devel` is
Berkeley DB 4.8, which needs `--with-incompatible-bdb` and can't open existing wallets. See [Berkeley DB](#berkeley-db).

## Portable Linux Builds

A build made through [depends](/depends/README.md) on an old distribution runs on newer ones. The result depends on
the glibc of the build system:

| Built on | Needs | Tested and working on |
| --- | --- | --- |
| AlmaLinux 8 (glibc 2.28) | glibc 2.28, GLIBCXX 3.4.22 | Every distribution in the [table above](#tested-distributions) |
| Ubuntu 20.04 (glibc 2.31) | glibc 2.29, GLIBCXX 3.4.26 | The same, except AlmaLinux 8 |

Neither runs on Ubuntu 18.04 (glibc 2.27). On Debian 10 (glibc 2.28), the AlmaLinux 8 build's `validityd` starts, but
nothing else could be tested because its package repositories are gone. On AlmaLinux 10, the GUI's libraries load but
the GUI itself couldn't be started, because Xvfb isn't available there.

Build tools on Ubuntu 20.04:

    sudo apt-get install build-essential make automake cmake curl ca-certificates libtool binutils bsdmainutils pkg-config python3 patch bison xz-utils

Build tools on AlmaLinux 8, after enabling `powertools` and EPEL as above:

    sudo dnf install gcc-toolset-13-gcc-c++ gcc-toolset-13-binutils make automake autoconf cmake curl libtool binutils pkgconf-pkg-config python3 patch bison xz diffutils which findutils file tar gzip bzip2 perl-interpreter
    source /opt/rh/gcc-toolset-13/enable

Then build, including Qt and Berkeley DB 6.2:

```bash
make -C depends HOST=x86_64-pc-linux-gnu NO_UPNP=1 NO_NATPMP=1 NO_USDT=1 -j8
./autogen.sh
CONFIG_SITE=$PWD/depends/x86_64-pc-linux-gnu/share/config.site ./configure --prefix=/ --with-gui=qt5 \
    --disable-tests --disable-bench --disable-fuzz-binary --without-miniupnpc --without-natpmp
make -j8
```

### Running a portable build

`validityd` and the command line tools only need glibc and libstdc++. `validity-qt` also needs these system libraries,
which desktop systems usually have most of:

Ubuntu, Debian & Linux Mint:

    sudo apt install libfontconfig1 libfreetype6 libxcb1 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-render0 libxcb-shape0 libxcb-shm0 libxcb-sync1 libxcb-xfixes0 libxcb-xinerama0 libxcb-xkb1 libxkbcommon0 libxkbcommon-x11-0 fonts-dejavu-core

Fedora, AlmaLinux, Rocky Linux & RHEL (on AlmaLinux 10, install `epel-release` first):

    sudo dnf install fontconfig freetype libxcb xcb-util-wm xcb-util-image xcb-util-keysyms xcb-util-renderutil libxkbcommon libxkbcommon-x11 dejavu-sans-fonts libstdc++

Arch Linux:

    sudo pacman -Syu --needed fontconfig freetype2 libxcb xcb-util-wm xcb-util-image xcb-util-keysyms xcb-util-renderutil libxkbcommon libxkbcommon-x11 ttf-dejavu

openSUSE:

    sudo zypper install fontconfig libfreetype6 libxcb1 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-render0 libxcb-shape0 libxcb-shm0 libxcb-sync1 libxcb-xfixes0 libxcb-xinerama0 libxcb-xkb1 libxkbcommon0 libxkbcommon-x11-0 dejavu-fonts libstdc++6

The portable `validity-qt` only has Qt's X11 (`xcb`) platform plugin. On a Wayland desktop it needs XWayland, and on a
system with no display it can't start at all, including `validity-qt -version`. Use `validityd` on servers.

## Dependencies

See [dependencies.md](dependencies.md) for a complete overview, and
[depends](/depends/README.md) on how to compile them yourself, if you wish to
not use the packages of your Linux distribution.

### Berkeley DB

The legacy wallet uses Berkeley DB. Validity 13.x and the release binaries use
Berkeley DB 6.2, and Linux distributions package older versions (5.3, or 4.8 on
openSUSE). What a build can open depends on which one it uses:

| Wallet made with | Build with distro BDB 5.3 | Build with BDB 6.2, or release binary |
| --- | --- | --- |
| Validity 13.x (BDB 6.2) | No | Yes |
| Release binary or BDB 6.2 build | No | Yes |
| Build with distro BDB 5.3 | Yes | Yes |

Opening a BDB 5.3 wallet with a BDB 6.2 build doesn't convert it, so the BDB 5.3
build can still open it afterwards.

**Warning**: A build with an older Berkeley DB reports a good wallet from BDB 6.2 as
corrupt, and suggests salvaging it with `validity-wallet`. The real cause, in the
wallet's `db.log`, is `unsupported database version 10`. Don't salvage the wallet.
Open it with a BDB 6.2 build instead.

A plain `./configure` that only finds an older Berkeley DB builds without legacy
wallet support. Pass `--with-incompatible-bdb` to use it anyway.

To use Berkeley DB 6.2, build it yourself. It needs `curl` to download the source. If
you don't want to use any other libraries built in depends, you can do:
```bash
make -C depends NO_BOOST=1 NO_LIBEVENT=1 NO_QT=1 NO_SQLITE=1 NO_NATPMP=1 NO_UPNP=1 NO_ZMQ=1 NO_USDT=1
...
to: /path/to/validity/depends/x86_64-pc-linux-gnu
```
and configure using the following:
```bash
export BDB_PREFIX="/path/to/validity/depends/x86_64-pc-linux-gnu"

./configure \
    BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-6.2" \
    BDB_CFLAGS="-I${BDB_PREFIX}/include"
```

**Note**: Make sure that `BDB_PREFIX` is an absolute path.

**Note**: You only need Berkeley DB if the legacy wallet is enabled (see [*Disable-wallet mode*](#disable-wallet-mode)).

Disable-wallet mode
--------------------
When the intention is to only run a P2P node, without a wallet, Validity can
be compiled in disable-wallet mode with:

    ./configure --disable-wallet

In this case there is no dependency on SQLite or Berkeley DB.

Mining is also possible in disable-wallet mode using the `getblocktemplate` RPC call.

Additional Configure Flags
--------------------------
A list of additional configure flags can be displayed with:

    ./configure --help


Setup and Build Example: Arch Linux
-----------------------------------
This example lists the steps necessary to setup and build a command line only distribution of the latest changes on Arch Linux:

    pacman --sync --needed autoconf automake boost gcc git libevent libtool make pkgconf python sqlite
    git clone https://github.com/RadiumCore/Validity.git
    cd Validity/
    ./autogen.sh
    ./configure
    make check
    ./src/validityd

If you intend to work with legacy Berkeley DB wallets, see [Berkeley DB](#berkeley-db) section.

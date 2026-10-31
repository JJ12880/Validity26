# macOS Build Guide

**Tested on macOS 14.7 with Xcode 16.2 on Apple Silicon. Release builds target macOS 11.0 or later.**

This guide describes how to build validityd, command-line utilities, and GUI on macOS

For binaries you plan to distribute, use the [release build](#release-build-using-depends). It is also the only way to get the Berkeley DB 6.2 that legacy wallets need.

## Preparation

The commands in this guide should be executed in a Terminal application.
macOS comes with a built-in Terminal located in:

```
/Applications/Utilities/Terminal.app
```

### 1. Xcode Command Line Tools

The Xcode Command Line Tools are a collection of build tools for macOS.
These tools must be installed in order to build Validity from source.

To install, run the following command from your terminal:

``` bash
xcode-select --install
```

Upon running the command, you should see a popup appear.
Click on `Install` to continue the installation process.

### 2. Homebrew Package Manager

Homebrew is a package manager for macOS that allows one to install packages from the command line easily.
While several package managers are available for macOS, this guide will focus on Homebrew as it is the most popular.
Since the examples in this guide which walk through the installation of a package will use Homebrew, it is recommended that you install it to follow along.
Otherwise, you can adapt the commands to your package manager of choice.

To install the Homebrew package manager, see: https://brew.sh

Note: If you run into issues while installing Homebrew or pulling packages, refer to [Homebrew's troubleshooting page](https://docs.brew.sh/Troubleshooting).

### 3. Install Required Dependencies

The first step is to download the required dependencies.
These dependencies represent the packages required to get a barebones installation up and running.

See [dependencies.md](dependencies.md) for a complete overview.

To install, run the following from your terminal:

``` bash
brew install automake libtool boost pkg-config libevent
```

### 4. Clone Validity repository

`git` should already be installed by default on your system.
Now that all the required dependencies are installed, let's clone the Validity repository to a directory.
All build scripts and commands will run from this directory.

``` bash
git clone https://github.com/JJ12880/Validity26.git
```

### 5. Install Optional Dependencies

#### Wallet Dependencies

It is not necessary to build wallet functionality to run `validityd` or  `validity-qt`.

###### Descriptor Wallet Support

`sqlite` is required to support for descriptor wallets.

macOS ships with a useable `sqlite` package, meaning you don't need to
install anything.

###### Legacy Wallet Support

Legacy wallets need Berkeley DB 6.2. Validity 13.x wallets are in BDB 6.2 format, and older versions can't open them.
Homebrew has no 6.2 formula, so build it through depends: see the [release build](#release-build-using-depends).
Skip if you don't intend to use legacy wallets.
---

#### GUI Dependencies

###### Qt

Validity includes a GUI built with the cross-platform Qt Framework.
To compile the GUI, we need to install `qt@5`.
Skip if you don't intend to use the GUI.

``` bash
brew install qt@5
```

Note: Building with Qt binaries downloaded from the Qt website is not officially supported.
See the notes in [#7714](https://github.com/bitcoin/bitcoin/issues/7714).

###### qrencode

The GUI can encode addresses in a QR Code. To build in QR support for the GUI, install `qrencode`.
Skip if not using the GUI or don't want QR code functionality.

``` bash
brew install qrencode
```
---

#### Port Mapping Dependencies

###### miniupnpc

miniupnpc may be used for UPnP port mapping.
Skip if you do not need this functionality.

``` bash
brew install miniupnpc
```

###### libnatpmp

libnatpmp may be used for NAT-PMP port mapping.
Skip if you do not need this functionality.

``` bash
brew install libnatpmp
```

Note: UPnP and NAT-PMP support will be compiled in and disabled by default.
Check out the [further configuration](#further-configuration) section for more information.

---

#### ZMQ Dependencies

Support for ZMQ notifications requires the following dependency.
Skip if you do not need ZMQ functionality.

``` bash
brew install zeromq
```

ZMQ is automatically compiled in and enabled if the dependency is detected.
Check out the [further configuration](#further-configuration) section for more information.

For more information on ZMQ, see: [zmq.md](zmq.md)

---

#### Test Suite Dependencies

There is an included test suite that is useful for testing code changes when developing.
To run the test suite (recommended), you will need to have Python 3 installed:

``` bash
brew install python
```

---

#### Deploy Dependencies

You can deploy a `.zip` containing the Validity application using `make deploy`.
It is required that you have `python` installed.

## Building Validity

### 1. Configuration

There are many ways to configure Validity, here are a few common examples:

##### Wallet (BDB + SQlite) Support, No GUI:

If Berkeley DB 6.2 is found, then legacy wallet support will be built.
If it is not found, `configure` only warns and builds without legacy wallet support, so check that the summary says `with bdb = yes`.
If `sqlite` is installed, then descriptor wallet support will also be built.
Additionally, this explicitly disables the GUI.

``` bash
./autogen.sh
./configure --with-gui=no
```

##### Wallet (only SQlite) and GUI Support:

This explicitly enables the GUI and disables legacy wallet support.
If `qt` is not installed, this will throw an error.
If `sqlite` is installed then descriptor wallet functionality will be built.
If `sqlite` is not installed, then wallet functionality will be disabled.

``` bash
./autogen.sh
./configure --without-bdb --with-gui=yes
```

##### No Wallet or GUI

``` bash
./autogen.sh
./configure --without-wallet --with-gui=no
```

##### Further Configuration

You may want to dig deeper into the configuration options to achieve your desired behavior.
Examine the output of the following command for a full list of configuration options:

``` bash
./configure -help
```

### 2. Compile

After configuration, you are ready to compile.
Run the following in your terminal to compile Validity:

``` bash
make        # use "-j N" here for N parallel jobs
make check  # Run tests if Python 3 is available
```

### 3. Deploy (optional)

You can also create a `.zip` containing the `.app` bundle by running the following command:

``` bash
make deploy
```

This produces `Validity.zip`, which contains only `Validity-Qt.app`, not the command-line tools.

## Release build using depends

This builds Qt 5.15.10, Berkeley DB 6.2.38 and the other libraries from source and links them statically, so the binaries only need macOS system libraries at runtime.

You need Xcode, plus `automake`, `libtool` and `pkg-config` from Homebrew. Check that `brew list --versions qt@5` prints nothing: when `qt@5` is installed, `configure` adds it to `PKG_CONFIG_PATH`, and the build could link against it.

``` bash
make -C depends NO_UPNP=1 NO_NATPMP=1 NO_USDT=1 -j6
./autogen.sh
CONFIG_SITE=$PWD/depends/aarch64-apple-darwin23.6.0/share/config.site ./configure --disable-tests --disable-bench --disable-fuzz-binary --without-miniupnpc --without-natpmp
make -j6
make deploy
```

The folder name under `depends` includes your Darwin version, so `aarch64-apple-darwin23.6.0` may differ on your machine.

Before going further, check that:

- the `configure` summary says `with bdb = yes` and `with gui / qt = yes`
- `otool -L src/validityd src/qt/validity-qt` lists only paths under `/usr/lib` and `/System/Library`

For the command-line tools, which `make deploy` leaves out, run this. The binaries end up in `stage/validity-26.2.0/bin`.

``` bash
make install-strip DESTDIR=$PWD/stage prefix=/validity-26.2.0
```

### Intel build

Use a separate clone so the two builds don't overwrite each other, and build the same way with the Intel host:

``` bash
make -C depends HOST=x86_64-apple-darwin NO_UPNP=1 NO_NATPMP=1 NO_USDT=1 -j6
./autogen.sh
CONFIG_SITE=$PWD/depends/x86_64-apple-darwin/share/config.site ./configure --disable-tests --disable-bench --disable-fuzz-binary --without-miniupnpc --without-natpmp
make -j6
make deploy
```

This has been tested on Apple Silicon with Rosetta installed, not on an Intel Mac.

### Opening a downloaded build

These builds are ad-hoc signed and not notarized, so macOS blocks them after download with the message "Apple cannot check it for malicious software". To allow the app, run:

``` bash
xattr -dr com.apple.quarantine /path/to/Validity-Qt.app
```

## Running Validity

Validity should now be available at `./src/validityd`.
If you compiled support for the GUI, it should be available at `./src/qt/validity-qt`.

The first time you run `validityd` or `validity-qt`, it will start downloading the blockchain.
This process could take many hours, or even days on slower than average systems.

By default, blockchain and wallet data files will be stored in:

``` bash
/Users/${USER}/Library/Application Support/Validity/
```

Before running, you may create an empty configuration file:

```shell
mkdir -p "/Users/${USER}/Library/Application Support/Validity"

touch "/Users/${USER}/Library/Application Support/Validity/validity.conf"

chmod 600 "/Users/${USER}/Library/Application Support/Validity/validity.conf"
```

You can monitor the download process by looking at the debug.log file:

```shell
tail -f $HOME/Library/Application\ Support/Validity/debug.log
```

## Other commands:

```shell
./src/validityd -daemon      # Starts the validity daemon.
./src/validity-cli --help    # Outputs a list of command-line options.
./src/validity-cli help      # Outputs a list of RPC commands when the daemon is running.
./src/qt/validity-qt -server # Starts the validity-qt server mode, allows validity-cli control
```

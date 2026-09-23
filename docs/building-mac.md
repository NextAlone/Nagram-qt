## Build instructions for macOS

### Prepare folder

Choose a folder for the future build, for example **/Users/user/TBuild**. It will be named ***BuildPath*** in the rest of this document. All commands will be launched from Terminal.

**Note about disk space:** The full build process will require approximately **55 GB** of free space. This includes:
- **~35 GB** for libraries (when building for both x64 and arm64 architectures)
- **~20 GB** for the compiled Telegram app (in the `out` folder)

### Obtain your API credentials

You will require **api_id** and **api_hash** to access the Telegram API servers. To learn how to obtain them [click here][api_credentials].

### Clone source code and prepare libraries

Go to ***BuildPath*** and run

    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    brew install git automake libtool cmake wget pkg-config gnu-tar ninja nasm meson

    sudo xcode-select -s /Applications/Xcode.app/Contents/Developer

    git clone --recursive https://github.com/telegramdesktop/tdesktop.git
    ./tdesktop/Telegram/build/prepare/mac.sh

### Building the project

Go to ***BuildPath*/tdesktop/Telegram** and run (using [your **api_id** and **api_hash**](#obtain-your-api-credentials))

    ./configure.sh -D TDESKTOP_API_ID=YOUR_API_ID -D TDESKTOP_API_HASH=YOUR_API_HASH

Then launch Xcode, open ***BuildPath*/tdesktop/out/Telegram.xcodeproj** and build for Debug / Release.

### Local Nagram Debug build on Apple Silicon

The Xcode workflow above expects a fresh `out` directory. For this checkout's
local Debug build, use a separate Ninja directory so its generator and compiler
cache do not conflict with `out`. Run the following from the repository root
after preparing `../Libraries`. Set the Qt version to the directory actually
installed under `../Libraries/local` (currently `Qt-6.11.1`):

```bash
env -u NIX_CFLAGS_COMPILE QT=6.11.1 cmake -S . -B out/macos-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_OBJC_COMPILER=/usr/bin/clang \
  -DCMAKE_OBJCXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_SYSROOT=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DCMAKE_DISABLE_PRECOMPILE_HEADERS=OFF \
  -DCMAKE_CXX_FLAGS=-DMETA_NO_STD_FORWARD_DECLARATIONS \
  -DCMAKE_OBJCXX_FLAGS=-DMETA_NO_STD_FORWARD_DECLARATIONS \
  -DTDESKTOP_API_ID=YOUR_API_ID \
  -DTDESKTOP_API_HASH=YOUR_API_HASH
```

Only set `QT` for the first configuration of a new build directory. For later
reconfiguration, preserve the cached Qt version:

```bash
env -u QT -u NIX_CFLAGS_COMPILE cmake -S . -B out/macos-debug
```

Build incrementally with:

```bash
env -u QT -u NIX_CFLAGS_COMPILE cmake --build out/macos-debug --target Telegram -j 6
```

The resulting app is `out/macos-debug/Nagram.app`. For a local test DMG:

```bash
hdiutil create -volname Nagram -srcfolder out/macos-debug/Nagram.app -format UDZO -ov out/macos-debug/Nagram-debug.dmg
hdiutil verify out/macos-debug/Nagram-debug.dmg
```

This Debug DMG has only an ad-hoc app signature and is not notarized for
distribution. The current `libtlottie.a` contains objects built for macOS 26
while this build targets macOS 11; a successful link does not establish
compatibility with older macOS versions. The repository's
`Telegram/build/build.sh` is a separate production workflow requiring release
credentials and signing identities.

[api_credentials]: api_credentials.md

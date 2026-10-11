# July Bangla Keyboard for Linux (preview)

An IBus engine (`ibus-engine-july-bangla`) around the shared C++ engine in `../src/engine`.
It types the same text for the same keys as on Windows and Android.

- **Modes:** বাংলা (Unicode), ক্লাসিক (SutonnyMJ) and English. Switch with Ctrl+Alt+B or
  from the input-method menu in the top bar. The mode is remembered in
  `~/.config/july-bangla/mode`.
- **Pre-edit:** the open syllable is shown underlined until it is finished, exactly like the
  composition on Windows.
- **Privacy:** no network access; only the mode is saved.

## Install (Ubuntu, Debian)

```
sudo apt install ./JulyBanglaKeyboard-linux-amd64.deb
```

Log out and log in again. Then open Settings → Keyboard → Input Sources → **+** →
Bangla → **July Bangla Keyboard**. Switch input sources with Super+Space.

Remove it with `sudo apt remove ibus-july-bangla`.

## Build

You need CMake, a C++20 compiler, Ninja and the IBus development files
(`sudo apt install cmake g++ ninja-build pkg-config libibus-1.0-dev`).

```
cmake -S linux -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux && ctest --test-dir build-linux
cd build-linux && cpack --config CPackConfig.cmake      # -> JulyBanglaKeyboard-linux-amd64.deb
```

`ctest` runs the shared engine's golden tests and an engine self-test that uses Linux key
codes. For development, run `build-linux/ibus-engine-july-bangla` without arguments: it
registers itself with the running IBus.

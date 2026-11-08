<h1 align="center">
Bloons 2 · PSVita Port
</h1>
<p align="center">
  <a href="#setup-instructions-for-players">How to install</a> •
  <a href="#controls">Controls</a> •
  <a href="#known-issues">Known Issues</a> •
  <a href="#build-instructions-for-developers">How to compile</a> •
  <a href="#how-it-works">How it works</a> •
  <a href="#credits">Credits</a> •
  <a href="#license">License</a>
</p>

Bloons 2 is a dart-throwing puzzle game from Ninja Kiwi, released on Android
by Miniclip: throw darts, pop the bloons, and clear each level with as few
darts as you can.

This repository contains a loader of **the Android release of Bloons 2 v1.0.0**,
based on the [Android SO Loader by TheFloW][gtasa] and modelled after
[mc3-vita][mc3]. The loader provides a tailored, minimalistic Android-like
environment to run the official game executable on the PS Vita.

Disclaimer
----------------

**Bloons** is a trademark of Ninja Kiwi. Bloons 2 for Android is published by
Miniclip. All rights reserved.

The work presented in this repository is not "official" or produced or
sanctioned by the owner(s) of the aforementioned trademark(s) or any other
registered trademark mentioned in this repository.

This software does not contain the original code, executables, assets, or
other non-redistributable parts of the original game product. The authors of
this work do not promote or condone piracy in any way. To launch and play
the game on their PS Vita device, users must possess their own legally obtained
copy of the game in the form of an .apk file.

Setup Instructions (For Players)
----------------

In order to properly install the game, you'll have to follow these steps
precisely:

- (Recommended) Make sure that you are either on 3.60 enso or 3.65 enso
  firmware version.

- Install or update [kubridge][kubridge] and [FdFix][fdfix] by copying
  `kubridge.skprx` and `fd_fix.skprx` to your taiHEN plugins folder
  (usually `ur0:tai`) and adding two entries to your `config.txt` under `*KERNEL`:

```
  *KERNEL
  ur0:tai/kubridge.skprx
  ur0:tai/fd_fix.skprx
```

```diff
! ⚠️ Don't install `fd_fix.skprx` if you're using the rePatch plugin!
- ⚠️ kubridge v0.3.1 or newer is required.
```

- Make sure you have `libshacccg.suprx` in the `ur0:/data/` folder on your
  console. If you don't, use [ShaRKBR33D][shrkbrd] to get it quickly and easily.

- <u>Legally</u> obtain your copy of Bloons 2 for Android in a form of an
  `.apk` file. This port is tailored for **v1.0.0** of the game. Other versions
  may work too but no support is provided for them.

> ℹ️ Verify that your build is the correct one using **sha1sum** (can also
> be found as an online tool). sha1sum for `lib/armeabi/libbloons2.so`
> **must** be `4e10ae9eb511c647fc02a4d4ac923e3d01f67882`

- Open the `.apk` with any zip explorer (like [7-Zip](https://www.7-zip.org/))
  and extract the file `lib/armeabi/libbloons2.so` into `ux0:data/bloons2/` on
  your Vita. Example of correct resulting path: `ux0:data/bloons2/libbloons2.so`

- Copy the `.apk` itself to `ux0:data/bloons2/` and rename it to `game.apk`.
  Example of correct resulting path: `ux0:data/bloons2/game.apk`. The game
  reads its textures, sounds and music straight from it; level and font data
  is unpacked automatically to `ux0:data/bloons2/files/` on first launch.

- Install `Bloons2.vpk` (from [Releases][latest-release], or build it
  yourself as described below).

- (Optional) For trophies to be unlockable, install [NoTrpDrm][notrpdrm]:
  copy `NoTrpDrm.suprx` to `ux0:tai/` and add it to your `config.txt` under
  `*main`:

```
  *main
  ux0:tai/NoTrpDrm.suprx
```

Your progress is saved to `ux0:data/bloons2/prefs.txt` and
`ux0:data/bloons2/files/`.

### Bringing over your Android save (optional)

You can carry over your progress, and anything your copy of the game
recorded as purchased, from your phone. The game keeps its save in
`/data/data/com.miniclip.bloons2/`, which normally needs a rooted phone (or
a backup tool that can extract app data) to copy.

1. From that folder, copy `shared_prefs/GAME_INFO.xml` and the `files`
   folder to `ux0:data/bloons2/import/` on your Vita:

```
  ux0:data/bloons2/import/shared_prefs/GAME_INFO.xml
  ux0:data/bloons2/import/files/...
```

2. Start the game. It imports the save once, shows how much it imported,
   and renames the folder to `import_done`.

Controls
-----------------

Levels are played with the buttons, slingshot-style like Angry Birds on PSP:
pull the left stick to aim (the game shows its aiming arrow as you pull), let
go of the stick to keep the aim, and press Cross to throw. Menus use the
touchscreen, with button shortcuts. The touchscreen works everywhere.

**In a level**

|       Button        | Action                                                          |
|:-------------------:|:----------------------------------------------------------------|
|      ![joysl]       | Pull to aim; the aim stays when you let go of the stick         |
|      ![cross]       | Throw                                                           |
|      ![circl]       | Cancel the aim (pause menu when not aiming)                     |
|      ![dpad]        | *Precise mode:* left/right adjust the angle, up/down the power  |
|      ![joysr]       | *Precise mode:* analog fine-tuning of angle and power           |
|      ![start]       | Pause menu                                                      |

In Precise mode your last aim is kept after every throw: press Cross straight
away to repeat the same shot, or nudge it with the d-pad first.

**In menus**

|       Button        | Action                                               |
|:-------------------:|:-----------------------------------------------------|
|    Touchscreen      | Tap buttons                                          |
| ![trigl] / ![trigr] | Swipe left / right (browse zones, spin the planet)   |
| ![circl] / ![start] | Back                                                 |

**Settings**

On first launch the port writes `ux0:data/bloons2/settings.txt`. Edit it on
your memory card and restart the game:

| Setting        | Values           | Meaning                                                      |
|----------------|------------------|--------------------------------------------------------------|
| `aim_mode`     | `0` / `1`        | `0` = Classic (stick only), `1` = Precise (default)           |
| `invert_aim`   | `0` / `1`        | Flip the aiming direction                                     |
| `aim_strength` | `50` - `200`     | How far a full stick tilt pulls, in percent (default `100`)  |

The port tells levels and menus apart from the screen changes the game writes
to its log. If that ever fails, the stick still aims everywhere and the L/R
swipe shortcuts are disabled.

Trophies
-----------------

The port turns the game's 17 achievements (the same Game Center set as the
iOS version) into Vita trophies, plus a platinum. They require
[NoTrpDrm][notrpdrm] (see setup above).

| Trophy           | Type     | How to get it                    |
|------------------|----------|----------------------------------|
| Monkey Legend    | Platinum | Unlock all other trophies.       |
| Million Pops     | Gold     | Pop 100k Bloons.                 |
| Bloon Baron      | Silver   | Launch 100 monkey aces.          |
| 100%             | Bronze   | Pop every Bloon in a level.      |
| Swing and a Miss | Bronze   | Hit nothing.                     |
| Ace Monkey       | Gold     | Complete the game.               |
| Bloons of Steel  | Bronze   | Use a Super Monkey.              |
| Bloon Academy    | Bronze   | Use a solution.                  |
| Easter Egg       | Bronze   | Pop 25 Bloons in the main menu.  |
| Starfleet Bloon  | Bronze   | Spin the planet 50 times.        |
| Daisy Cutter     | Bronze   | Complete Popping Fields.         |
| Just Deserts     | Bronze   | Complete Bloon Dunes.            |
| Ice Demon        | Bronze   | Complete Frosty Fun.             |
| My Everest       | Bronze   | Complete Sneak Peaks.            |
| Jungle Fever     | Silver   | Complete Rubber Jungle.          |
| Temple of Bloon  | Silver   | Complete Temple of Bloon.        |
| Bloon BBQ        | Silver   | Complete Crazy Coast.            |
| Hot Stuff        | Silver   | Complete Mount Magma.            |

The trophy pack (`extras/trophies`) is generated, icons included, by
`extras/scripts/trophies/make_trophies.ps1`.

Known Issues
----------------

1. Everything online is gone: ads, the news feed, OpenFeint, Facebook,
   Tapjoy/GetJar offers and in-app purchases are stubbed out and do nothing.
   Content locked behind in-app purchases stays locked.
2. Android dialogs with two buttons are shown as the system Yes/No dialog;
   the message tells you which of the game's buttons "Yes" and "No" stand for.
3. "Bloon Academy" needs a solution, which the game may only hand out
   through in-app purchases.
4. This port hasn't been verified on real hardware yet. If the game crashes,
   build in Debug mode and check the log (see below).

Build Instructions (For Developers)
----------------

In order to build the loader, you'll need a [vitasdk](https://github.com/vitasdk)
build fully compiled with softfp usage (`libbloons2.so` uses the soft-float
ABI). The easiest way to obtain one is following the instructions on
https://vitasdk.org/ while replacing the URL in this command:

```bash
git clone https://github.com/vitasdk/vdpm
```

Like this:

```bash
git clone https://github.com/vitasdk-softfp/vdpm
```

All the required libraries (vitaGL, libzip, zlib, OpenSSL, …) should get
installed automatically if you follow the installation process from
https://vitasdk.org/.

After all these requirements are met, you can compile the loader with the
following commands:

```bash
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Debug # Or =Release if you don't want debug logging
cmake --build build -j$(nproc)
```

Build options (pass with `-D<OPTION>=<VALUE>`):

| Option               | Default              | Description                                              |
|----------------------|----------------------|----------------------------------------------------------|
| `DATA_PATH`          | `ux0:data/bloons2/`  | Where the game files live on the Vita                    |
| `USE_HIGHRES_ASSETS` | `ON`                 | Use the game's high resolution sprite sheets             |
| `SKIP_SO_HASH_CHECK` | `OFF`                | Don't warn when `libbloons2.so` isn't v1.0.0             |

This CMakeLists has the same convenience targets as mc3-vita:

```bash
cmake --build build --target send # Build, upload eboot.bin and run (requires vitacompanion)
cmake --build build --target dump # Fetch latest coredump and parse
```

In Debug builds, the loader and the game log to the console via
`sceClibPrintf` (view it with e.g. [PSVita-StubLogger][stublogger] or a
serial/UART setup).

How it works
----------------

Bloons 2 is a cocos2d-based game compiled with Miniclip's "Coco" Objective-C
runtime into a single native library, `libbloons2.so`. The Java side of the
Android app is thin: it creates a GLES 1.x surface, forwards touches, and
provides services back to native code through static methods of
`com.miniclip.nativeJNI.cocojava`. The loader replaces that Java side:

| Source                  | What it does                                                         |
|-------------------------|----------------------------------------------------------------------|
| `source/main.c`         | Startup sequence (same order of `CocoJNI` calls as the Java code), main loop |
| `source/game/input.c`   | Touchscreen passthrough and the button controls (slingshot aiming, menu shortcuts) |
| `source/game/context.c` | Tells levels from menus using the game's own log output              |
| `source/game/settings.c`| `settings.txt` (aiming mode, inversion, strength)                    |
| `source/game/achievements.c` | Maps the game's Game Center achievements to trophies (`source/utils/trophies.c`) |
| `source/java.c`         | [FalsoJNI][falsojni] implementation of the `cocojava` methods the game calls |
| `source/dynlib.c`       | Table of all 313 libc/libm/zlib/GLES/libdl imports of the `.so`      |
| `source/game/apk.c`     | Reads assets from the original `.apk` (stands in for `AssetManager`) |
| `source/game/texture.c` | Decodes PNGs with stb_image for `loadTexture` → `MnativeInitBitmap`  |
| `source/game/audio.c`   | SceAudioOut mixer replacing `SoundPool` / `MediaPlayer` (MP3 via dr_mp3) |
| `source/game/prefs.c`   | Persistent replacement for `SharedPreferences`                       |
| `source/reimpl/`        | bionic ↔ newlib compatibility (stdio streams, stat, errno, pthreads, …) |

Credits
----------------

- [Andy "The FloW" Nguyen][flow] for the original .so loader.
- [Rinnegatamante][rinne] for vitaGL and countless loader improvements.
- [Volodymyr Atamanenko][v-atamanenko] for [mc3-vita][mc3], which this port is
  modelled after, and [FalsoJNI][falsojni].
- [Sean Barrett][stb] for stb_image and [David Reid][drlibs] for dr_mp3.

License
----------------

This software may be modified and distributed under the terms of the GNU
General Public License v3. See the [LICENSE](LICENSE) file for details.

Files taken from other projects keep their original licenses, noted in their
headers: so_util, FalsoJNI, pthread-svelte and the reimplementations from
mc3-vita are MIT; stb_image and dr_mp3 are public domain / MIT-0.

[cross]: https://raw.githubusercontent.com/v-atamanenko/mc3-vita/master/extras/icons/cross.svg "Cross"
[circl]: https://raw.githubusercontent.com/v-atamanenko/mc3-vita/master/extras/icons/circle.svg "Circle"
[joysl]: https://raw.githubusercontent.com/v-atamanenko/mc3-vita/master/extras/icons/joystick-left.svg "Left Joystick"
[joysr]: https://raw.githubusercontent.com/v-atamanenko/mc3-vita/master/extras/icons/joystick-right.svg "Right Joystick"
[dpad]: https://raw.githubusercontent.com/v-atamanenko/mc3-vita/master/extras/icons/dpad-top-down.svg "D-Pad"
[selec]: https://raw.githubusercontent.com/v-atamanenko/mc3-vita/master/extras/icons/dpad-select.svg "Select"
[start]: https://raw.githubusercontent.com/v-atamanenko/mc3-vita/master/extras/icons/dpad-start.svg "Start"
[trigl]: https://raw.githubusercontent.com/v-atamanenko/mc3-vita/master/extras/icons/trigger-left.svg "Left Trigger"
[trigr]: https://raw.githubusercontent.com/v-atamanenko/mc3-vita/master/extras/icons/trigger-right.svg "Right Trigger"
[notrpdrm]: https://github.com/Rinnegatamante/NoTrpDrm

[gtasa]: https://github.com/TheOfficialFloW/gtasa_vita
[mc3]: https://github.com/v-atamanenko/mc3-vita
[falsojni]: https://github.com/v-atamanenko/FalsoJNI
[kubridge]: https://github.com/bythos14/kubridge/releases/
[fdfix]: https://github.com/TheOfficialFloW/FdFix/releases/
[shrkbrd]: https://github.com/Rinnegatamante/ShaRKBR33D/releases/latest
[latest-release]: ../../releases/latest
[stublogger]: https://github.com/Princess-of-Sleeping/PSVita-StubLogger

[flow]: https://github.com/TheOfficialFloW/
[rinne]: https://github.com/Rinnegatamante/
[v-atamanenko]: https://github.com/v-atamanenko/
[stb]: https://github.com/nothings/stb
[drlibs]: https://github.com/mackron/dr_libs

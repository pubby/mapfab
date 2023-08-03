# MapFab

MapFab is a level editor for creating (new) NES games.

- **Website:** http://pubby.games/mapfab.html
- **Documentation:** http://pubby.games/mapfab/doc.html
- **Discord:** https://discord.gg/RUrYmC5ZeE

## License

- MapFab is licensed under GPL 3.0 (see `COPYING`).

## Building

**Requirements:**
- [GCC Compiler](https://gcc.gnu.org/), supporting the C++20 standard.
- [wxWidgets](https://www.wxwidgets.org/)
- [Make](https://www.gnu.org/software/make/)

MapFab can be built in either debug mode, or release mode.
- Debug mode includes runtime sanity checks to ensure the compiler is working correctly.
- Release mode has no checks and is optimized for speed.

To build in debug mode, run:

    make debug

To build in release mode, run:

    make release


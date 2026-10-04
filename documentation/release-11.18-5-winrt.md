# WineGDK 11.18-5-winrt

Speed up repeat GDK launches with Xodus 0.7.2 and fix network initialization when the game starts before Xbox authentication would previously have completed. Release names now use `winrt` to reflect both GDK and UWP support.

- Restore validated, account-bound Xbox bootstrap state from Xodus, including its matching proof key. Respect credential expiration, title/client identity and forced refresh; fall back to normal authentication with older brokers or invalid cache data.
- Remove the artificial 2.5-second network initialization delay. Deliver an initial connectivity notification to each registered listener through its task queue, preserving unregister/cancellation behavior. Marketplace, server discovery and friend-world connectivity were confirmed working after this change.
- Implement shell-focus WNF queries/subscriptions used by the GameInput service and missing COM/RPC proxy thunks. Add GDK error-handler support.
- Add `WINEDBG_LOG` to send automatic crash reports to a file or Unix socket without a debugger console. Add `WINEBOOT_LOG` for silent prefix progress and `WINE_STARTUP_LOG` for startup milestones; see `documentation/xodus-launcher.md` in the source repository.
- Retain direct launch of extracted UWP applications and the bundled graphics/runtime companions from the previous release.

On the test host, time from Wine process start to the first visible GDK window fell from 3.24 seconds on a fresh authentication path to 0.67 seconds with a cached session. This measures the first window, not completion of game loading or login; network and host timing vary.

Validation includes 32/64-bit Xbox-session, connectivity, WNF/RPC and GDK error probes, launcher logging tests, and fresh/cached GDK game sessions. Online services remain work in progress.

Extract the archive and run `bin/wine /path/to/game.exe`, optionally with a dedicated `WINEPREFIX`. Use a running, authenticated Xodus service for Xbox login. Existing prefixes can be updated with `bin/wineboot -u` after their applications are closed.

The Linux x86_64 archive includes 64-bit and WoW64 Windows support, native graphics/runtime companions and their license notices. No game files, prefixes or credentials are included.

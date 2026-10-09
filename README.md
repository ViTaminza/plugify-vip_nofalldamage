# vip_nofalldamage (Plugify)

No fall damage for VIPs: the 'nofalldamage' feature (groups.ini, on by default in the production groups) makes fall damage below 4999.0 harmless. The original used the Pisex utils damage pre-hook; the port detours the same engine function through the polyhook plugin (signature from the plugin's own gamedata.jsonc, resolved with s2sdk) and blocks DMG_FALL hits for enabled slots. A port of the ABKAM module '[VIP] No Fall Damage' (vip_nofalldamage).

Plugify port (fork) of **VIP_NoFallDamage ([VIP] No Fall Damage)** by ABKAM (Metamod (C++)).
Original: https://github.com/Pisex/cs2-vip-modules
The code was rewritten for Plugify / s2sdk; behaviour follows the original.

## Requirements
- [Plugify](https://github.com/untrustedmodders/plugify) with [s2sdk](https://github.com/untrustedmodders/plugify-plugin-s2sdk)
- Plugify dependencies: s2sdk, vip, polyhook, hudmenu (optional)

## Installation
Download `vip_nofalldamage-1.2.1.zip` from Releases and extract it into `game/csgo/`.

## Building
Requires Docker. `./build.sh` builds in the Steam Runtime 3 (sniper) SDK image; the package
is written to `build/package/vip_nofalldamage`.

`plugins/vip` carry only the API headers of the plugins this one uses.

## License
GPL-3.0, see LICENSE.


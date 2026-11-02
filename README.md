# vip_nofalldamage (Plugify)

No fall damage for VIPs: the 'nofalldamage' feature (groups.ini) makes fall damage below 4999.0 harmless. The original used the Pisex utils damage pre-hook; the port detours the same engine function through the polyhook plugin (signature from the plugin's own gamedata.jsonc, resolved with s2sdk) and blocks DMG_FALL hits for enabled slots. Pure s2sdk: the map gate and the schema-offset cache live in the plugin itself, no pisexcompat. A port of the ABKAM module '[VIP] No Fall Damage' (vip_nofalldamage).

Plugify port (fork) of **VIP_NoFallDamage ([VIP] No Fall Damage)** by ABKAM (Metamod (C++)).
Original: https://github.com/Pisex/cs2-vip-modules
The code was rewritten for Plugify / s2sdk.

## Requirements
- [Plugify](https://github.com/untrustedmodders/plugify) with [s2sdk](https://github.com/untrustedmodders/plugify-plugin-s2sdk)
- Plugin version: 1.3.0
- Plugify dependencies: s2sdk (>=2.18.2), vip (>=1.0.0), polyhook (>=2.5.2)
- Self-contained s2sdk integration: uses s2sdk directly, without the pisexcompat library.

## Installation
Download `vip_nofalldamage-1.3.0.zip` from Releases and extract it into `game/csgo/`.

## Building
Requires Docker. `./build.sh` builds in the Steam Runtime 3 (sniper) SDK image; the package
is written to `build/package/vip_nofalldamage`.

`plugins/vip` carry only the API headers of the plugins this one uses.

## License
GPL-3.0, see LICENSE.


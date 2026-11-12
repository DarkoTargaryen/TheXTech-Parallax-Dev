# SMBX2 Starman port — implementation reference

Source supplied: SMBX2 installation archive, inspected 2026-11-12.

## Located SMBX2 files
- `scripts/npcs/AI/starman.lua` — lifecycle, duration, NPC collision kills, shader/particles, music.
- `scripts/npcs/npc-293.lua` — standard Starman NPC (32×32, 2 frames, 13-second duration, bouncing movement).
- `scripts/npcs/npc-559.lua` — alternate Starman.
- `scripts/shaders/npc/starman.frag` — animated rainbow tint.
- `graphics/particles/starman_sparkle.png` and `scripts/particles/p_starman_sparkle.ini`.
- `sound/extended/starman.ogg`.

## Requested gameplay (different from SMBX2's per-player timer)
1. One Starman in a multiplayer block, never one item per player.
2. Collecting the star gives **every active player** one shared 13-second timer.
3. Visual invincibility for each active character (including mounts).
4. Touch damage kills eligible enemies, excluding protected NPC categories.
5. One shared star music channel with restoration and ending warning.
6. Respawning players receive the remaining duration.
7. Level exit resets all Starman state; no lingering invulnerability or music.
8. Existing power-ups and multiplayer item spawning remain unchanged.
9. Implement and test on branch `starman` only; Windows **x64** builds only.

## Engine notes
- TheXTech currently defines NPC IDs through 292; `maxNPCType` is 300, leaving space for ID 293.
- SMBX2 Lua event hooks (`onPostNPCCollect`, `onTick`, `onDraw`) do not exist as drop-in equivalents here; adapt the logic to C++ pickup/collision, update, and render passes.
- Reusing the existing `Player_t::Immune` as the star timer would interfere with ordinary post-hit invulnerability. Introduce separate shared Starman state and immunity checks.
- NPC 293 requires a trait registration, editor/runtime visuals and collectible handling; simply adding its enum value is insufficient.
- Avoid copying SMBX2's per-player music management; this project requires one group-wide effect.

## Verification checklist
- 1, 2, 3, and 4 players; pickups while dead/respawning; repeated star pickup refreshes timer.
- Mario, Luigi, Peach, Toad, Link; Yoshi, shoes and clown cars.
- Protected NPCs and boss interactions; no friendly-NPC kills.
- Invulnerable character contact and normal hazards/instant-death exceptions.
- Pause and level exit, music resumption, last-second warning.
- x64 compile, available automated tests, then interactive gameplay testing.

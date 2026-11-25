# AI visibility and target recovery

The uploaded capture confirms the gas-god-ammo build, all six update hooks,
modern/legacy gas hooks and owned weapon hooks. Gas blocks reached nine. The
captured gameplay includes non-master sessions with locally created clones.
Those state summaries do not expose every AI latch; they do not establish a
single conclusive cause for every reported attack stall.

The same cached ARM64 dump.cs, global-metadata.dat and libil2cpp.so used for the
previous build remain available read-only. Exact hashes are recorded in
`game-abi-audit.json`. No reference game binary is shipped in this archive.
Addresses below are evidence, never runtime hook configuration.

## Native target visibility

- HandleLocalSeeTargetToggling.hpg(bool on, bool network), 0x253BFD0.
  With network=false it calls GameObject.SetActive on grannySeeTarget.
  With network=true it invokes the existing RPC with a single boxed Boolean.
- RPC_SyncToggleGrannySeeTarget(bool), 0x253C110: applies SetActive to that
  player's serialized grannySeeTarget GameObject. It does not hide the model.
- hph/hpi, 0x253C214/0x253C220: native show/hide events call hpg(true,true)
  and hpg(false,true). Keeping this path also reaches an unmodified host.

The implementation matches the owned toggle PhotonView to the current local
inventory PhotonView and requires the target to be a proper child of the
owner transform. A player root/model cannot be disabled. Only that own target
is changed. Normal visibility requests during God are remembered for restore;
self-received visibility RPCs cannot unhide it early. Local state is read back.
Native RPC call success is not a remote application acknowledgement. A bounded
three-second refresh covers late peers/host changes. Context/ownership loss
prevents writes to a stale or newly remote character.

## Per-instance recovery

- EnemyEye.eli (0x279F290) and EnemyEyeGrandpa.dwz (0x2774380) clear their
  selected Player and following-name state.
- EnemyAIGrannyMulti.eim (0x2790F9C) and EnemyAIGrandpaMulti.duo (0x27671D0)
  reset native combat latches. ein (0x27912F0) / dup (0x2767524) choose a native
  patrol target. The scene setup functions eil/dun are not replayed.
- SpiderMomAIController.inb (0x258AC14) clears target-player/teddy tracking,
  hunting and the following name, choosing an existing waypoint when present.
  inf (0x258984C) resets its animation Boolean parameters.

Recovery requires the real master, owned active enemy and matching scene/room.
Only an inactive target or our known invisible local victim qualifies. Active
visible victims, paused categories, genuine weapon hits/death/freeze are left
alone. Native per-instance cleanup is followed by ordinary perception; route
lists are not rebuilt and the original AI/clone creation paths remain intact.
Local contact hooks and MomGrab reject only our protected victim before native
attack latches are entered. Other victims and God-off calls are forwarded.

The complete native AI still runs on the real host. Client-owned clone patrol
is not changed into a second authoritative attack controller. An already stuck
AI on a remote unmodified host cannot be reset by these local recovery calls.
Start a fresh room and enable God before chasing/attack to test prevention.

## UI removal

The respawn override command, snapshot state, hooks and UI are removed. Native
respawn timing is untouched. The enemy settings panel is removed. Clone
categories have a separate select/create flow and a PauseEnemy command, which
sets only the selected kind's paused flag. Every discovered original and clone
of that kind is covered by the existing update/trigger/pose/animation pause
mechanism. Non-master pause remains local.

# Gas explosion hook evidence

The source is based exclusively on the attached progression-hamarat ZIP.
A cached ARM64 game reference was used read-only to verify the new death ABI;
its exact hashes are recorded in game-abi-audit.json. Reference game binaries
are not added to this source archive. Runtime code resolves metadata names,
checks field types and validates the message value size/alignment; it does
not configure hooks with fixed RVAs or field offsets.

ARM64 reference evidence:
- HandlePlayerChestExplosion.OnEnable (0x2545710) subscribes the own-player
  explosion callback to ExplosionTrigger.dig (0x274E978).
- hup(object, EventArgs), 0x2546534: builds isChest=false and sends the normal
  death RPC. This is the gas callback; huo (chest callback) is left intact.
- RPC_SyncPlayerDeathExplosion(bool, PhotonMessageInfo), 0x2546644: publishes
  the victim death event and disables the visible player/target before moving
  the death camera. Guarding only PlayerState/DaysManager is too late for these
  visual effects, so the own-victim non-chest RPC is intercepted at entry.
- PhotonMessageInfo: int timeInt, padding, Player Sender, PhotonView photonView;
  24-byte aggregate with 8-byte alignment on AArch64. Existing project MessageInfo
  is reused. The native disassembly receives the aggregate indirectly in x2.
- Legacy playerController.RPC_playerExplosionDeath and
  RPC_playPlayerBurningOnDeath have no managed parameters.

The gas can's owner/sender is deliberately not used to infer the victim. The
receiver component's PhotonView must equal the existing rooted local state
manager's PhotonView. The original call is forwarded unchanged for remote
victims, chest RPCs, God off, stale world/ownership, and off-thread execution.
Neither Explosion nor ExplosionTrigger is hooked; their world effects and
other victims continue through the native game paths.

# Clone door / noise / fall integration

Reference files: supplied ARM64 libil2cpp.so, dump.cs, global-metadata.dat.
No native offsets or RVAs are used by the runtime integration.

- EnemyEyeGrandpa has grandpaUpdateParseRate and no daysManager. The previous
  shared Granny requirement prevented its native sight check. Both human AI and
  Eye now use the clone's registered PhotonView. Remapping tolerates differing
  sibling order only when a unique named child path exists. Cameras outside the
  clone hierarchy are rejected, and bounded binding retries are logged.
- Mom preparation now repairs its local/scene references rather than reporting
  ready immediately. Sight checks use isPlayerInView, not merely hunting.
- Real host sight checks preserve native target selection/attacks. After two
  seconds without sight a controlled, non-attacking/non-wounded clone uses
  EnemyEye.eli / EnemyEyeGrandpa.dwz / Mom.inb; human eim/duo and ein/dup or
  Mom.inf perform per-instance cleanup and existing patrol selection.
- Door.ewl (0x2479398) issues the normal existing door RPC. Door animation and
  collision/obstacle updates stay native. The first Physics.Linecast collider
  must have a Door ancestor; no global door search or lock override. Flags are
  type-checked and all clones share a bounded per-door two-second cooldown.
  Linecast arity 5 is unique in the reference metadata. RaycastHit storage size
  and alignment come from class_value_size, then native get_collider is invoked
  on unboxed storage. No guessed collider or hit-data offsets.
- Newly active NoiseObjectFallsMulti / Grandpa objects provide native sound
  positions. Existing startup markers are baselined; repeated and paired markers
  are deduplicated. An idle host-controlled clone within 20 metres investigates
  for at most ten seconds, waits two seconds on arrival, then uses native patrol.
  Destination is applied only to its own agent after FixedUpdate. No shared
  waypoint, global event, clone prefab, or player target transform is modified.
  This bridge consumes native noise markers, not arbitrary ambient AudioSources.
- HandleFallDemage.Update calls hnk tracking and hnl landing evaluation. hnl
  compares rfe (Single) with damage/death times and calls hnm/hno. The owned God
  player's verified rfe is zeroed before hnl; hnm/hno are separately guarded.
  hnl still executes grounding/landing bookkeeping, while God-off/remote/other
  scene/thread/ownership calls forward. Ghost's existing Update guard remains.

Latest supplied log (gmm-gas-god-20261122-231258-867.zip): originals were removed
before clone creation (actors=0/clones=0). Both human clones then showed
behavior=0, senses=0, despite movement and native frames. Every bind retry failed.
Mom performed sight checks and selected a player. The source-actor prerequisite
in PrepareCloneBehavior explains the rejected setup in this no-original case;
no particular remaining native scene defect can be inferred from those logs.

Current repair:
- Clone-local hierarchy, registered view, back-reference, Camera/Animator and
  typed singleton DaysManager no longer depend on an alive original actor.
  An available original is optional repair input. No Start/OnEnable replay.
- Native NoiseObjectFallsMulti.ffp / Grandpa.fga callbacks retain IEnumerator
  return values and GC roots while recording the accepted noise. Polling remains
  a fallback. FootStepsController.OnSteppedFloor is observed with original sound
  behavior retained; steps use an eight-metre radius, object noise twenty metres.
  Stronger noise supersedes a nearby quieter step rather than being deduplicated.
- Owned healthy clone no-motion recovery after twelve seconds and attack-latch
  recovery after fifteen seconds use native per-instance patrol routines. Category
  pause, weapon-hit/death flags, lost world/ownership/host prevent recovery. No
  scene actor revival, teleport or global event is used.
- FirstPersonController.walkSpeed/crouchSpeed (verified Single fields) control the
  own player speed menu. Native code reads these at 0x2532B08 / 0x2532B18. Values
  are not multiplied cumulatively; game base changes and normal restoration are
  tracked. No camera, gravity, animation-time or global timeScale modification.
- Sense diagnostics now name the exact missing view, scene manager, camera,
  hierarchy, player list or native invocation prerequisite.

No physical Android game is connected. Native geometry, remote event application
and real combat should be retested in a fresh room, including the no-original
scenario. Game tests must keep God off when expecting enemies to see the user.

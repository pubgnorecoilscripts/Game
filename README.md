# PARASITE

A 5v5 multiplayer social-infiltration prototype for Unreal Engine 5.

You are a small alien parasite. You can possess chairs, bins, vending machines,
shopping carts, cars, mall shoppers — and, for eight seconds, an enemy player.
Both teams hide a nest somewhere in an abandoned shopping mall. Find the enemy
nest, sit in it for twenty uninterrupted seconds, and it is yours.

## How this project is built

The rules of the game live in `Source/Parasite/Core/` as **plain C++ with no
Unreal dependency at all** — possession, cooldowns, hijacking, scans, nests,
DNA, upgrades, teams, phases and the win condition are one class,
`Parasite::FMatchSim`.

Everything under `Source/Parasite/` is a thin shell around it. Each tick the
game mode pushes actor positions into the simulation, ticks it, and mirrors the
result back onto pawns and replicated properties. Clients only ever ask.

That split is the point: the interesting half of the game can be compiled and
tested without an engine install, and it is, on every change:

```
./Tests/run_tests.sh          # 245 assertions over 24 scenarios, no Unreal needed
python3 Tools/check_project.py # replication, input, RPC and asset wiring
```

## Requirements

* Unreal Engine **5.4** — targeted and configured for 5.4.x
  (`"EngineAssociation": "5.4"` covers 5.4.4).
  The code deliberately avoids engine APIs that moved between 5.x versions, so it
  should build on 5.3 and 5.5+ as well.
* No marketplace or project assets. Every mesh, material, light and sound is
  either engine basic content (`/Engine/BasicShapes`) or generated at runtime.

## Building

```
# Windows - builds, then prints the lines that explain any failure
Tools\build_windows.bat

# Windows, engine installed somewhere else
Tools\build_windows.bat "D:\Epic\UE_5.4"

# Linux / macOS
"$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" ParasiteEditor Linux Development -Project="$PWD/Parasite.uproject"
```

Then open `Parasite.uproject`, or run it directly:

```
UnrealEditor Parasite.uproject -game -log
```

There is no `.umap` to open. The mall, its lighting, the props, the shoppers and
the nests are all built in code, so the project boots on `/Engine/Maps/Entry` and
`AParasiteGameState::BeginPlay` builds the world on every machine.

## Playing

The front end appears on launch.

* **PLAY** — start a match on the local server (works solo for testing).
* **HOST** — restart as a listen server, up to 10 players.
* **JOIN** — type an address (default `127.0.0.1`) and connect.
* **SETTINGS** — mouse sensitivity and the control list.
* **QUIT**

To test multiplayer in the editor, set *Number of Players* to 2–10 and *Net Mode*
to *Play As Listen Server*.

### Controls

| Key | Action |
| --- | --- |
| WASD | Move |
| Shift / Ctrl / Space | Sprint / crouch / jump (Space swings a door you are possessing) |
| E | Possess, or open a door you are standing next to |
| Q | Leave the current host |
| LMB | Parasite leap toward a nearby host |
| F | Parasite scan (20 s cooldown) |
| MMB | Ping a location for your team |
| R | Mash to resist an enemy riding you |
| 1 / 2 / 3 | Evolve: Jumper, Mimic, Infiltrator (60 DNA, 2 per match) |
| Tab | Scoreboard |
| Esc | Menu |

## Rules

Every number below lives in one struct, `Parasite::FRules`, and the tests run a
match on a compressed timeline by overriding it.

| Rule | Value |
| --- | --- |
| Possession range | 3 m (+2.5 m with Jumper) |
| Object / NPC / enemy-player possession | 30 s / 45 s / 8 s (+4 s with Mimic) |
| Possession cooldown | 3 s |
| Scan | 20 s cooldown, 22 m radius, 1.5 s reveal (×0.45 against Infiltrator) |
| Nest infection | 20 uninterrupted seconds; expulsion refunds 35 % to the defenders |
| Match | 15 minutes; highest infection wins, team DNA breaks a tie |
| Players | up to 10, balanced automatically; 2v2 through 5v5 all work |

## Layout

| Path | Responsibility |
| --- | --- |
| `Core/ParasiteRules.h` | Enums, value types and every tunable number |
| `Core/MatchSim.*` | The authoritative match: all rules, no engine |
| `Tests/CoreTests.cpp` | 24 scenarios driving the simulation directly |
| `ParasiteGameMode` | The only bridge: pushes the world in, mirrors results out |
| `PossessableComponent` | Registers an actor as a host, replicates its view |
| `PossessablePawn` | Every mall object; behaviour comes from its mobility profile |
| `ParasiteNPC` | Shopper with a waypoint idle brain, possessable |
| `ParasiteCharacter` | The parasite, and the host for enemy-player hijacks |
| `ParasitePlayerController` | Input and requests; decides nothing itself |
| `ParasiteGameState` / `ParasitePlayerState` | Replicated views of the simulation |
| `ParasiteNest` | The organic growth; a view of the simulation's nest |
| `MallBuilder` | Builds the mall: scenery locally, gameplay actors on the server |
| `ParasiteHUD` | Canvas HUD, front end, scoreboard, end screen |
| `ParasiteAudio` | Runtime tone synthesiser (no audio assets) |

## A note on enemy possession

Riding an enemy deliberately does **not** transfer their pawn. The victim keeps
their controller, their connection and their state; the pawn is flagged
`bHijacked` and takes movement from the attacker's server RPC instead of its
owner's input. They can mash **R** to force the parasite out early, and it
expires on its own after eight seconds. There is no code path that gives one
player lasting control of another.

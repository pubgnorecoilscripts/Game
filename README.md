# PARASITE

A 5v5 multiplayer social-infiltration prototype for Unreal Engine 5.

You are a small alien parasite. You can possess chairs, bins, vending machines,
shopping carts, cars, mall shoppers — and, for eight seconds, an enemy player.
Two teams each hide a nest somewhere in an abandoned shopping mall. Find the
enemy nest, sit in it for twenty uninterrupted seconds, and it is yours.

## Requirements

* Unreal Engine **5.3** (the project is written against the 5.3 API; the one
  version-sensitive override, `APlayerController::InputKey`, is guarded for 5.6+).
* No marketplace or project assets. Every mesh, material, light and sound is
  either engine basic content (`/Engine/BasicShapes`) or generated at runtime.

## Building

```
# Windows
"C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\Build.bat" ParasiteEditor Win64 Development -Project="%CD%\Parasite.uproject" -WaitMutex

# Linux / macOS
"$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" ParasiteEditor Linux Development -Project="$PWD/Parasite.uproject"
```

Then open `Parasite.uproject`, or run the packaged/editor game directly:

```
UnrealEditor Parasite.uproject -game -log
```

There is no `.umap` to open: the mall, the lights, the props, the NPCs and the
nests are all built in code, so the project boots on `/Engine/Maps/Entry` and
`AParasiteGameState::BeginPlay` builds the world on every machine.

## Playing

The front end appears on launch.

* **PLAY** — start a match on the local server (works solo for testing).
* **HOST** — restarts the map as a listen server, up to 10 players.
* **JOIN** — type an address (default `127.0.0.1`) and connect.
* **SETTINGS** — mouse sensitivity and the control list.
* **QUIT**

To test multiplayer in the editor, set *Number of Players* to 2–10 and *Net Mode*
to *Play As Listen Server*.

### Controls

| Key | Action |
| --- | --- |
| WASD | Move |
| Shift / Ctrl / Space | Sprint / crouch / jump (Space opens a door while possessing one) |
| E | Possess, or interact with a nearby door |
| Q | Leave the current host |
| LMB | Parasite leap toward a nearby host |
| F | Parasite scan (20 s cooldown) |
| MMB | Ping a location for your team |
| R | Mash to resist an enemy riding you |
| 1 / 2 / 3 | Evolve: Jumper, Mimic, Infiltrator (60 DNA, 2 per match) |
| Tab | Scoreboard |
| Esc | Menu |

## Rules

| Rule | Value |
| --- | --- |
| Possession range | 3 m (+2.5 m with Jumper) |
| Object / NPC / enemy-player possession | 30 s / 45 s / 8 s (+4 s with Mimic) |
| Possession cooldown | 3 s |
| Scan | 20 s cooldown, 22 m radius, 1.5 s reveal (×0.45 vs. Infiltrator) |
| Nest infection | 20 uninterrupted seconds; expulsion refunds 35 % to the defenders |
| Match | 15 minutes; highest infection wins, team DNA breaks a tie |

## Architecture

| File | Responsibility |
| --- | --- |
| `ParasiteTypes.h` | Enums and all balance constants |
| `PossessableComponent` | Makes any actor a host; owns the replicated possession state |
| `PossessablePawn` | Every mall object; behaviour comes from its mobility profile |
| `ParasiteNPC` | Shopper with a waypoint idle brain, possessable |
| `ParasiteCharacter` | The parasite itself, and the host for enemy-player hijacks |
| `ParasitePlayerController` | All player intent; every action is a server RPC |
| `ParasiteGameMode` | Teams, phases, nests, scoring, win condition, rematch |
| `ParasiteGameState` | Replicated phase, timer and infection |
| `ParasiteNest` | Infection zone, defensive pulse |
| `MallBuilder` | Builds the mall; static scenery locally, gameplay actors on the server |
| `ParasiteHUD` | Canvas HUD, front end, scoreboard, end screen |
| `ParasiteAudio` | Runtime tone synthesiser (no audio assets) |

Everything that matters is server authoritative: teams, possession, cooldowns,
infection, DNA, the timer and the result. Clients only ask.

Enemy-player possession deliberately does **not** transfer the pawn. The victim
keeps their controller and their connection; the pawn is flagged `bHijacked` and
takes movement from the attacker's server RPC instead of its owner's input, and
the victim can mash **R** to force the parasite out early.

## Checks

`python3 Tools/check_project.py` verifies that every replicated property is
registered, every bound input exists in `DefaultInput.ini`, every RPC has an
implementation, and that no code references a non-engine asset.

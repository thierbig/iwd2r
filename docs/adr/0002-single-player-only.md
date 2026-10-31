# ADR 0002: Single player only

Status: accepted

## Context

The upstream engine includes multiplayer over DirectPlay: a lobby, session enumeration, modem and serial setup, a multiplayer screen (`CScreenMultiPlayer`, 3,637 lines) and a GameSpy stub. DirectPlay does not exist off Windows and is obsolete on it. The decompiled `CNetwork` already contains the single-player path the original uses: a null service provider (`SERV_PROV_NULL`) under which sessions open and players are created without DirectPlay, sends return success without sending, and receiving is skipped.

## Decision

Multiplayer is removed permanently. It cannot sit behind an option, so it is recorded as a deviation in the README.

- The DirectPlay side of `CNetwork` is deleted. `CNetwork` keeps its name, header, state machine and every method kept code calls, with the null service provider as the only provider.
- `CScreenMultiPlayer` and the DirectPlay headers are deleted, along with the references to the screen. `CScreenConnection` stays as the start screen with its multiplayer entry hidden.
- `CMessage` is kept whole as the engine's action queue, which single player uses; a later project finishes it against the null-provider network.
- `CMultiplayerSettings` and `CChatBuffer` are kept because single-player screens load party permissions and chat text through them.
- `CGameSpy` is ported (PR #61) but stays inert: its sockets are the platform's, whose `WSAStartup` always fails, and its heartbeat only starts when a multiplayer TCP/IP session is hosted, which this engine never does.

## Alternatives considered

- Keep multiplayer behind a build option. Rejected: it would need a DirectPlay replacement and real network testing, and a half-working option misleads players.
- Replace DirectPlay with a new transport. Rejected for this program: a different protocol could not interoperate with the original and is a separate product.
- Delete `CNetwork` and `CMessage` outright. Rejected: kept code calls them in single player, and removal would rewrite call sites against ADR 0001.

## Consequences

- The simulation differs from the original only where the original's network code matters, which in single player is nowhere; the deviation is about missing features, not changed behavior.
- Project 2's oracle runs are single-player only, and they confirm the null-provider values against the vanilla executable.
- Methods whose bodies were entirely DirectPlay return `FALSE` or do nothing; any future multiplayer work starts from a clean transport interface rather than these stubs.
- Modding that depends on multiplayer sessions is not supported.

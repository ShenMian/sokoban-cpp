# sokoban-cpp

[![CodeFactor](https://www.codefactor.io/repository/github/shenmian/sokoban-cpp/badge)](https://www.codefactor.io/repository/github/shenmian/sokoban-cpp)

A simple sokoban, using [SFML] framework.  

<p align="center"><img src="docs/screenshot.png" width=70%></p>

## Features

- Mouse control: move character, select & drop crates.
- Undo/undo all.
- Automatically save and restore session.
- Autosave best solutions.
- Save opened levels.
- Show dead crates: freeze deadlocks detection.
- Rotate level map.
- Resize map to fit window.

## Keymap

| Key                        | Action                            |
| -------------------------- | --------------------------------- |
| `W`/`A`/`S`/`D`            | Move the character                |
| `Up`/`Down`/`Left`/`Right` | Move the character                |
| `H`/`J`/`K`/`L`            | Move the character                |
| `Esc`                      | Reload current level              |
| `BackSpace`                | Single step undo                  |
| `R`                        | Rotate map clockwise              |
| `-`/`=`                    | Switch to the previous/next level |
| `P`                        | Replay solution                   |
| `Ctrl` + `I`               | Switch instant move               |
| `Ctrl` + `V`               | Import level from clipboard       |

## Assets

- Image from [Kenney].
- Sound effect/music from [sinneschlösen]/[Pixabay].

[SFML]: https://github.com/SFML/SFML
[Kenney]: https://www.kenney.nl/assets/sokoban
[sinneschlösen]: https://pixabay.com/users/sinneschlösen-1888724/?utm_source=link-attribution&amp;utm_medium=referral&amp;utm_campaign=music&amp;utm_content=117362
[Pixabay]: https://pixabay.com/sound-effects/?utm_source=link-attribution&amp;utm_medium=referral&amp;utm_campaign=music&amp;utm_content=6297

#!/bin/bash
# Play the game by typed commands (for playtesting): tools/play.sh SAVEFILE "COMMANDS" [PICTURE.png]
# The game is kept in SAVEFILE (a new game if it does not exist yet); HELP lists the commands.
# With a third argument, a picture of what you see afterwards is written there.
cd "$(dirname "$0")/.." || exit 1
save="$1"; cmds="$2"; pic="$3"
if [ -z "$save" ] || [ -z "$cmds" ]; then echo "usage: tools/play.sh SAVEFILE \"COMMANDS\" [PICTURE.png]"; exit 1; fi
# guards: commands not in quotes (each word after the first became a picture's name, dropped in the
# game's folder: AROUND, SAY:give_letter ...), a picture not named .png, a save with no folder
if [ $# -gt 3 ]; then echo "Put all the commands in one pair of quotes: tools/play.sh SAVEFILE \"WALKTO:x TALK:x SAY:job\" [PICTURE.png]"; exit 1; fi
if [ -n "$pic" ] && [ "${pic%.png}" = "$pic" ]; then echo "The picture's name must end in .png (\"$pic\" looks like a command: put all the commands in one pair of quotes)"; exit 1; fi
case "$save" in */*) ;; *) echo "Give the save with its folder (DIR/NAME_save.txt), not a bare name: it would be made in the game's own folder"; exit 1;; esac
case "$save" in saves/autosave.txt|*/saves/autosave.txt) echo "not the saved game of the window (saves/autosave.txt)"; exit 1;; esac
args=(--play "$save" --walk "$cmds" --size 1440x900)
[ -e "$save" ] || args+=(--creator)   # a new game begins on the choosing screen, as in the window
[ -n "$pic" ] && args+=(--shot "$pic")
# each message once: the status line {X} is followed by the same X as a plain line
./galileo "${args[@]}" 2>&1 | grep -v -E '^world built|^  art |^continuing the saved game|^saved the game|^wrote .*chunks drawn' |
    awk '{ if ($0 ~ /^  \{.*\}$/) { last = substr($0, 4, length($0) - 4); print; next } if ($0 ~ /^  \[scene /) { print; next } if (last != "" && $0 == "  " last) next; last = ""; print }'

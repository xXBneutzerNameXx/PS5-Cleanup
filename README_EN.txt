PS5 CLEANUP 1.1 by da_Chris86
=============================

Brings a jailbroken PS5 back to a freshly-set-up state - without
reinstalling and without a reset. The program never touches your account,
licenses, system software or the disc drive pairing. For all PS5 models
(PS5, Slim, Pro, each with disc drive or as Digital Edition).

WHICH FILE IS WHICH? - PLEASE READ FIRST!
-----------------------------------------
  ps5cleanup_v1.1.elf ...... the PS5 app. It starts in TEST MODE ("TEST
                            MODE" at the top): it shows everything and
                            deletes nothing. It only deletes once you
                            switch test mode off in the Options AND
                            explicitly confirm the deletion.
  Not sure, other model or other firmware? Leave test mode on and first
  take your time to look at what is offered.

IMPORTANT: NO "REBUILD DATABASE"
--------------------------------
- Sony's "Rebuild Database" (Safe Mode) deletes installed PS4 games from
  fake PKGs - they have to be installed again afterwards. With PS5 Cleanup
  the rebuild is not needed.
- NEVER choose "Reset PS5" or "Reinstall System Software".

TWO WAYS
  - Choose yourself: tick single entries - e.g. when one game or tool
    causes problems, delete only its data, saved data or trophies.
    Everything else stays. Nothing is ticked beforehand.
  - Clean up everything: everything on the internal SSD at once, like
    freshly set up. You add the M.2 and extended storage one by one.
    Beforehand it shows exactly how many games and entries are deleted on
    each drive and how much space is free afterwards.

TESTED on a PS5 Pro (CFI-7021) with firmware 12.00, M.2 and USB drive.
Other models and firmwares are not blocked but have not been tested yet -
there, please check in test mode first that the list is right.

FEEDBACK
  At the end PS5 Cleanup briefly asks whether everything worked - it is
  optional, "Without feedback" always works. If the PS5 is connected to
  the internet, it sends the feedback directly. Without internet it shows
  a QR code: scan it with your phone, tap "Worked" or "Did not work",
  done - no account and no PC needed. What is sent is shown on the right
  of that screen under "This will be sent" and here in DETAILS under
  "Feedback". For more detail use GitHub (issues or discussions) or
  psxtools.de - ideally with model, firmware and the log (see "Log"
  under DETAILS). This also works without deleting anything.


HOW TO USE (PS5 app, directly on the PS5)
-----------------------------------------
1. Load your jailbreak and start ps5cleanup_v1.1.elf with your payload
   loader or Payload Manager. No USB drive needed - if one is plugged in,
   the log is saved there.
   In the Payload Manager it also works as its own source: Settings ->
   Manage Sources -> Add Source, then enter:
   https://raw.githubusercontent.com/xXBneutzerNameXx/PS5-Cleanup/main/payloads.json
2. On first start the "PS5 Cleanup" tile appears. The start screen shows
   console, disc drive pairing and jailbreak at a glance.
3. Cross = "Scan". Afterwards you see all areas with their sizes. Test
   mode is on, nothing is deleted. If the list is right, switch test mode
   off in the Options.
4. Cross = "Choose yourself": tick entries, then Square = "Continue to
   delete". The "Confirm" screen shows on the left what will be deleted,
   on the right what stays in any case. To delete, hold Cross for 2
   seconds (releasing cancels).
   Or Triangle = "Clean up everything": add the M.2 or extended storage
   with Cross, type DELETE with Triangle, then Triangle = "Clean up".
5. At the end the result screen shows every entry. If anything is left
   over, the reason is shown right below it; "Try again" scans again and
   ticks what is left. Then Cross = restart (recommended) or Circle =
   quit - the optional feedback comes first. After the restart, start
   your jailbreak as usual. Do not rebuild the database (see above).

BUTTONS
  Cross ............ choose, tick, confirm
  Circle ........... back; on the start screen hold = quit
  Triangle ......... "Clean up everything"; in the list: all on/off
  Square ........... language (start screen); in the selection:
                     "Continue to delete"
  L1 / R1 .......... change area (games, apps, saved data ...); on the
                     start screen after scanning R1 = "Scan again"
  L2 / R2 .......... one page further
  Right / Left ..... into a block (choose parts) or back out
  Options .......... options; while scanning and deleting: log
  The button bar at the bottom always shows what works on that screen.

CONTROL IN A BROWSER (on your phone or PC)
  Everything also works in a browser - with mouse, keyboard or finger.
  The address is at the bottom right of the start screen (e.g.
  192.168.178.20:9100), in the Options under "Browser control" also as a
  QR code. If port 9100 is already in use, PS5 Cleanup takes the next
  free one up to 9109 - the exact address is always on the start screen.
  From outside your home only via a VPN into your home network.
  Phone or PC must be in the same home network as the PS5. No
  login, nothing to install.
  The page shows the same as the TV: the same steps, the same test mode,
  the same confirmations (hold "Delete" for 2 seconds, type DELETE for
  "Clean up everything"). What you tick in the browser is shown on the TV
  too; the TV shows "IN BROWSER" at the top while someone uses the
  browser. When PS5 Cleanup quits, the page is gone too.

UPDATES
  At start PS5 Cleanup checks whether there is a new version. If so, the
  top shows "New version ..." and the Options show "Get update"
  (Triangle). The new version is downloaded and checked (size, checksum,
  that it really is PS5 Cleanup) and goes where PS5 Cleanup is now.
  "Yes, replace" (recommended) deletes the old file only after the new
  one is fully checked; "No, keep both" puts it next to it. If anything
  goes wrong, the old version stays unchanged. Nothing is started
  automatically: quit PS5 Cleanup and then start the new file.


NEW IN 1.1
----------
- Completely new look on the TV: start screen with console, disc drive
  pairing and jailbreak, after scanning an overview of all areas with
  sizes. Selecting with tabs, every entry with tile image, location and
  size; on the right it says what will be deleted and what happens
  afterwards.
- Games and programs are shown as a block with their parts - open it and
  choose single parts. The block's header names exactly what its tick
  takes along.
- New confirmation screen: on the left what will be deleted, on the right
  what stays in any case and how much space is free afterwards. Scanning
  and deleting show the real progress ("entry x of y").
- "Clean up everything" with a preview per drive (games, entries, space)
  and a clear warning when games on the M.2 or extended storage are
  deleted too. Right before deleting, PS5 Cleanup compares once more with
  the real selection - if it is more than shown, it asks again.
- More thorough and safer: selected programs are closed before deleting,
  for all by the same rules. Tiles and game updates are removed by
  Sony's own uninstall, PS5 Cleanup waits for Sony's answer. Start folders
  of other apps under /system_ex/app are detected. If something only
  partly works, the result says "partly" - and if something is left
  over, the reason is shown right on the result screen.
- More detected, more images: programs without a tile get their image
  from the program file or from the payload manager's catalog. Download
  and data folders are listed with the right program. Playden: saved data
  with image and name from Playden's library.
- Get updates directly in the app (see UPDATES), send feedback directly
  from the app - without internet via QR code.
- Under the hood: old special paths removed, one common set of rules for
  all ways of deleting, every change secured by the automatic full test.

Older changes are listed on GitHub under Releases:
https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases


THE PROGRAM NEVER TOUCHES
-------------------------
- System software and disc drive pairing
- User account, activation and real licenses
- Sony system apps and system settings
The PS5 app changes Sony's databases only selectively: the entries of
deleted saved data, broken add-on information of a game you delete, and
tiles that a tool entered directly into Sony's app list (e.g. TabShifter)
and that you select for deletion.
Each of these changes is checked first and made completely or not at all.


WHAT CAN GO (you choose)
------------------------
[1] Games - on the internal SSD, M.2 and extended storage (USB hard
    drive), the location is shown. Game, update and every DLC can be
    selected individually (the game takes its update and DLC with it).
    Game images (exFAT etc.) and game files (.pkg) count as games.
[2] Apps - normal apps and homebrew tools: tile, below it its data and
    its ELF file (also on the USB drive)
[3] Saved data
[4] Trophies
[5] Screenshots & videos
[6] Other - updates and add-ons without a game, leftovers of deleted
    games, error reports, temporary files, unknown folders and anything
    where it is not certain who it belongs to
[7] Jailbreak - only what your jailbreak needs to start. Can only be
    selected individually, never part of "Clean up everything".
Empty areas are not shown. Entries outside the internal SSD (M.2,
extended storage, USB drive): if any are selected, there is a separate
confirmation. Files on a USB drive can only be selected individually.
With "Clean up everything" you add the M.2 and extended storage
yourself - otherwise they stay untouched.


IMPORTANT
---------
- NO "Rebuild Database" (see above). Whatever PS5 Cleanup had to leave
  unchanged is shown at the end with the reason on the result screen.
- Console suddenly turns itself off? Some modified PS4 packages (e.g.
  updates with DLC built in afterwards) contain broken add-on entries.
  When the PS5 reads them in the background, it crashes and turns off -
  even if the game is not running. PS5 Cleanup detects such games while
  scanning and shows "WARNING: broken add-on entries". Fix: delete this
  game with PS5 Cleanup (the entries are repaired first) and install it
  again with an unmodified update.
- Deleted data is gone for good. So is saved data without a cloud backup.
- Nothing is deleted until you confirm (hold Cross for 2 seconds or type
  DELETE) - and never in test mode. The safe choice is preselected in
  every question. If a size has not been counted yet, the program counts
  it first - NOTHING is deleted while it does. "Back to selection" =
  nothing happens.
- After "Clean up everything" you have to set up your jailbreak tools
  again. The jailbreak entry points (e.g. YouTube/Y2JB) are never part of
  it - they only go if you tick them individually.
- If the program reports an error while scanning (e.g. "could not be read
  completely"), nothing is deleted - just scan again. Better to cancel
  than to delete with an incomplete list.
- If an M.2 is installed but the program reports "no M.2 found": do not
  delete, restart the PS5 instead.
- Use at your own risk. Errors are possible. Back up
  anything important to you first.
- If there are problems: in the Options, "Save log" saves the log to the
  USB drive (Square shows it). Please send it to us (see "FEEDBACK" and
  "Log" under DETAILS).


DETAILS (only if you want to know exactly)
------------------------------------------

Disc drive pairing
  On Slim and Pro, the detachable disc drive is paired with the console.
  The program reads and shows the pairing status. It never touches system
  software or system settings. The original PS5 has a built-in drive
  without pairing, Digital Editions have none.
  The display tells apart: "Paired online", "Factory paired", "Paired",
  "No pairing", "Pairing not finished" and "Unpairing not finished"
  (started but not completed) and "Not queried" (no valid answer). The
  PS5 cannot tell whether a drive was ever paired with another console.

Closing programs
  If a running program belongs to what you delete, PS5 Cleanup closes it
  first - otherwise it would recreate deleted items right away. The same
  rules apply to all programs. Programs that only mounted other folders
  are left alone. The jailbreak and PS5 Cleanup itself always keep
  running.

ShadowMount games
  The menu only has a tile; the game itself is on the M.2, the internal
  SSD or the USB drive. If the game is deleted, the tile always goes with
  it. If only the tile is deleted, it comes back once ShadowMount is
  loaded again. If a game file sits behind a link, only the tile is
  offered - never the other file. While the Game Compressor is working,
  the affected game is locked.

M.2 SSD
  The folders on the M.2 (e.g. homebrew, games, downloads) are expanded:
  every game and file in them is its own entry. The folders themselves
  stay. An empty folder is only removed if it is still empty when deleting.

Sizes
  Right after the scan, the program counts the size of every entry in the
  background and shows it. "min." means: not fully counted (very large
  or not readable).

Trophies
  Each game is shown as one block. At the top the trophy list: the
  description of the trophies the game brings along (names, texts,
  images). Below it the trophy progress: which trophies an account has
  already earned - with several accounts, one per account. Both are
  stored in different places and can be ticked separately. Deleting
  progress: trophies synced with PSN come back the next time you start
  the game, never-synced ones are gone. "Clean up everything" does not
  delete trophy lists of games that stay (e.g. on a kept M.2).

Assignment (which data belongs to which tile?)
  Only what is clearly proven goes under a tile: folders with its ID,
  folders with its name (ignoring case, spaces and hyphens, "+" counts as
  "plus"), a file like xyz_startup.log next to its folder xyz - and
  whatever its program names itself. For this PS5 Cleanup reads the
  program files of the tiles (app folders, homebrew folders, payloads);
  nothing is changed. If two tiles fit equally well or a program could
  not be read completely, nothing is guessed: the entry stays under
  Other. Anything only assumed is listed in the app's block with its own
  checkbox and is never ticked automatically. ELF files are never
  assigned via another app's program - a program often names other tools
  it only uses.

Playden
  Steam games are recognised and removed together with their entry in
  Playden's game list; their saved data is listed with image and name
  under Saved data. Playden's own data is marked red, because without it
  Playden no longer starts. If Playden is in the middle of setting a game
  up, the game stays unchanged - let Playden finish, then delete again.

Jailbreak protection
  The entry points your jailbreak starts from are detected and never
  ticked automatically - neither by selecting the whole area nor by
  "Clean up everything". Ticking or deleting them shows a warning.
  Detected: YouTube (Y2JB), Netflix ('N Hack), Lua jailbreak (the known
  Lua games and their saves), PS2 save jailbreak (Okage, Star Wars Racer
  Revenge, Jak X), Ren'Py jailbreak (yarpe), WebKit Autoloader (tile and
  the browser's offline storage, also PSH5/Goldengames) and the browser
  storage. Only what the jailbreak needs to start is protected. Jailbreak
  tools (etaHEN, Payload Manager, autoloader settings, update packages,
  ELF files) stay selectable as usual.

Test mode
  At start, test mode is on and "TEST MODE" is shown at the top. The app
  shows everything like real deleting, but the delete buttons are locked -
  if you press them, the app tells you that test mode is on. To switch it
  off: Options, "Test mode". The app does not remember this.

Stopping a deletion
  While deleting, holding Circle for one second stops deleting after the
  current entry. An entry is never cut off halfway. All others stay
  unchanged and are listed that way on the result screen. PS5 Cleanup
  then does not remove itself, so you can continue later.

Result
  The result screen shows every entry: deleted, partly, unchanged or
  error. If anything is left over, the reason is shown right below it
  (e.g. "the game is running - close it first, then delete again").
  "Try again" scans again and ticks exactly what is left.

Log
  Options, "Save log" shows the log (Square) and saves it to the USB
  drive - at any time, even without deleting anything and in test mode.
  As soon as something is deleted, PS5 Cleanup also saves it as a file
  by itself (ps5cleanup_protokoll_<date>_<time>.txt): on the USB drive;
  if there is none, or it is full or not writable, on the internal
  storage in the folder /data/PS5-Cleanup-Protokolle; if that is full
  too, on the M.2 in the folder PS5-Cleanup-Protokolle. Every run gets
  its own file. The folders stay even when PS5 Cleanup removes itself at
  the end, and are never offered for deletion. Only "Clean up
  everything" removes old logs on the internal storage - never the
  current one, never those on the USB drive or the M.2. Fetch it e.g.
  over FTP.

Privacy - what PS5 Cleanup keeps and sends
  On the PS5, PS5 Cleanup keeps nothing except its own tile and its logs
  (USB drive, otherwise on the PS5) - no settings, no lists, nothing
  remembered between two starts.
  - Update notice: at start, PS5 Cleanup asks GitHub (api.github.com)
    once for the latest version. Only this request is sent - as when
    opening any website, GitHub sees the internet address. Without
    internet nothing happens.
  - Get update: only if you choose it in the Options. Then PS5 Cleanup
    downloads the new version from GitHub and checks it before saving it
    (see UPDATES). It is not run automatically.
  - Browser control: the page comes from the PS5 itself and only answers
    devices in the home network. Nothing goes to the internet. Nothing
    stays in the browser except the tile images in its cache (10 minutes
    at most).
  - Feedback: see below. Nothing goes out without your "Send".

Feedback (optional)
  The screen comes at the end before "Restart" or "Quit", and at any
  time via Options, "Give feedback". "Without feedback" always works.
  When the screen opens, the PS5 briefly checks whether it can reach the
  feedback server (only the request is sent). The right side of the
  screen shows the key parts of what would be sent. In total it is:
  - PS5 Cleanup version, normal or test version, build stamp
  - model number (e.g. CFI-7021), firmware, disc drive pairing status
  - jailbreak entry points set up (e.g. YouTube), connected storage (M.2,
    extended storage, USB drive), ShadowMount
  - numbers: entries per area, selected, deleted, errors, memory use,
    space freed, duration; whether "Choose yourself" or "Clean up
    everything"
  - for errors, up to 5 short codes: step, kind of entry, location and
    error code (e.g. "Sony uninstall, PS4 game, internal, 0x80990001")
  - your answer ("all good" or "something did not work") and, if you
    like, a short note (up to 200 characters)
  - with internet also the cleaned log. It contains names of games and
    apps and folder paths of your console; IP, e-mail and MAC addresses,
    serial numbers and the user ID are removed beforehand.
  Never included: accounts, passwords, IP and MAC addresses, serial
  number.
  With internet, the PS5 only sends once you choose "Send". Without
  internet it shows a QR code - it only holds the details and numbers,
  not the log (that stays on the USB drive). Nothing is stored until you
  tap "Send" on your phone. Personal details someone types into the text
  field (IP, e-mail and MAC addresses, serial numbers, long numbers) are
  already removed by the PS5 before sending - "Your note" shows exactly
  what is sent. As with any website visit, the server briefly sees the
  internet address - it is not stored. The page runs on Cloudflare:
  https://feedback.ps5cleanup.workers.dev (privacy notice there under
  /datenschutz).

System folder
  Homebrew tiles create an empty folder in the write-protected system
  area (/system_ex). So that PS5 Cleanup can remove it together with the
  tile, it makes this area writable, with the same command as ShadowMount,
  and leaves it that way until the next restart, as ShadowMount does
  too. The restart restores the write protection. Only empty folders of
  selected tiles and start folders that Sony first confirms are really
  left over are deleted there - never Sony's files.

Language
  The language follows the PS5 system language (German -> German,
  otherwise English). Square on the start screen switches it, also after
  scanning (then it scans again).

Files
  ps5cleanup_v1.1.elf          the PS5 app (starts in test mode)
  README_EN.txt                this guide (English)
  LIESMICH.txt                 this guide (German)
  LICENSE.txt                  licenses and components by other authors
  SHA256.txt                   checksums of the files
  The source code is on GitHub:
  https://github.com/xXBneutzerNameXx/PS5-Cleanup


LEGAL
-----
PS5 Cleanup is an unofficial, independent community project by da_Chris86.
It is not affiliated with, endorsed by, or sponsored by Sony Interactive
Entertainment. PlayStation and PS5 are trademarks of Sony Interactive
Entertainment Inc.

License: the PS5 app is licensed under the GNU General Public License v3.0
or later (GPL-3.0) - Copyright (c) 2026 da_Chris86. The code of the former
Windows version is licensed under the MIT License. Details and components
by other authors in LICENSE.txt.

Developed by da_Chris86 with Claude (Anthropic). Independent code review:
OpenAI Codex. Idea, requirements and the tests on the real console come
from da_Chris86.

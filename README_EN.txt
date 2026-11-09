PS5 CLEANUP 1.0.9 by da_Chris86
===============================

Brings a jailbroken PS5 back to a freshly-set-up state - without
reinstalling and without a reset. The program never touches your account,
licenses, system software or the disc drive pairing. For all PS5 models
(PS5, Slim, Pro, each with disc drive or as Digital Edition).

WHICH FILE IS WHICH? - PLEASE READ FIRST!
-----------------------------------------
  ps5cleanup_v1.0.9.elf ... the PS5 app. It starts in TEST MODE ("TEST
                            MODE" at the top): it shows everything and
                            deletes nothing. It only deletes once you
                            switch test mode off in the Options menu AND
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
  - Targeted: tick single entries - e.g. when one game or tool causes
    problems, delete only its data, saved data or trophies. Everything
    else stays. Button "Delete selection only".
  - Full: everything on the internal SSD at once, like freshly set up
    (the M.2 is asked separately). Button "Full cleanup".

TESTED on a PS5 Pro (CFI-7021) with firmware 12.00 and an M.2.
Other models and firmwares are not blocked but have not been tested yet -
there, please check in test mode first that the list is right.

FEEDBACK
  Easiest via QR code: at the end and under Options, "Log & feedback",
  PS5 Cleanup shows a QR code. Scan it with your phone, tap "It worked" or
  "Problem", done - no account and no PC needed. What is sent is
  described under "Feedback via QR code" in DETAILS. For more detail use
  GitHub (issues or discussions) or psxtools.de - ideally with model,
  firmware and the log (see "Log" under DETAILS). This also works without
  deleting anything.


HOW TO USE (PS5 app, directly on the PS5)
-----------------------------------------
1. Load your jailbreak and start ps5cleanup_v1.0.9.elf with your payload
   loader or Payload Manager. No USB drive needed - if one is plugged in,
   the log is saved there.
   In the Payload Manager it also works as its own source: Settings ->
   Manage Sources -> Add Source, then enter:
   https://raw.githubusercontent.com/xXBneutzerNameXx/PS5-Cleanup/main/payloads.json
2. On first start the "PS5 Cleanup" tile appears. The interface starts
   right away: use the controller (Cross = select, hold Circle = quit,
   Options = options and log).
3. "Scan" and take your time to look at what was found. Test mode is on,
   nothing is deleted. If the list is right, switch test mode off in the
   Options menu.
4. Tick entries and Square = delete selection only: on the screen "This
   will be deleted", move right to "Delete" and hold Cross for 1.5
   seconds (releasing cancels). Or Triangle = full cleanup: type DELETE.
5. At the end: Cross = restart (recommended, preselected) or
   Circle = quit. After the restart, start your jailbreak as usual.
   Do not rebuild the database (see above).

CONTROL IN A BROWSER (on your phone or PC)
  Everything also works in a browser - with mouse, keyboard or finger.
  The address is at the bottom of the start screen (e.g.
  http://192.168.178.20:9100), in the Options menu under "Browser
  control" also as a QR code. Phone or PC must be in the same home
  network as the PS5. No login, nothing to install.
  It is the same app as on the TV: the same steps, the same test mode,
  the same confirmations (hold "Delete" for 1.5 seconds, type DELETE for
  the full cleanup). What you tick in the browser is shown on the TV too;
  the TV shows "IN BROWSER" at the top while someone uses the browser.
  When PS5 Cleanup quits, the page is gone too.


NEW IN 1.0.9
------------
- Control in a browser on your phone or PC (see "CONTROL IN A BROWSER").
- Notice of new versions right in the app: at start, PS5 Cleanup asks
  GitHub for the latest version number. If there is a newer one, the
  start screen says so (see "Privacy" in DETAILS).
- As its own source in the Payload Manager (see "HOW TO USE"). New
  versions arrive there by themselves. Idea and template: Strongt1me.
- More game images with name and picture: .ffpkg, .ufs, .img (UFS2),
  .ffpfs, .ffpfsc and .pfs, also when the image is split into pieces,
  plus the picture from PS4 PKG files.
- Tile images in JPEG format are shown now too (e.g. the Payload
  Manager's).
- Better assignment: parts of ProsperoStore apps are listed with their
  app, data folders of known apps with their program. Empty capture
  folders are only offered when their app is gone.
- Tested on a PS5 Pro (CFI-7021), firmware 12.00, with M.2: deleting on
  the TV and in the browser, selected entries and full cleanup.

Older changes are listed on GitHub under Releases:
https://github.com/xXBneutzerNameXx/PS5-Cleanup/releases


THE PROGRAM NEVER TOUCHES
-------------------------
- System software and disc drive pairing
- User account, activation and real licenses
- Sony system apps and system settings
The PS5 app changes Sony's databases only selectively: the entries of
deleted saved data and broken add-on information of a game you delete.
Each of these changes is checked first and made completely or not at all.


WHAT CAN GO (you choose)
------------------------
[1] Games - on the internal SSD, M.2 and extended storage (USB hard
    drive), the location comes first.
    Game, update and every DLC can be selected individually (the game
    takes its update and DLC with it). Game images (exFAT etc.) count as
    games.
[2] Apps - normal apps and homebrew tools: tile, below it its data and
    its ELF file (also on the USB drive)
[3] Saved data
[4] Trophies
[5] Screenshots & videos
[6] Other - updates and add-ons without a game, leftovers of deleted
    games, error reports, temporary files, unknown folders and anything
    where it is not certain who it belongs to
[7] Jailbreak - only what your jailbreak needs to start. Can only be
    selected individually, never part of the full cleanup.
Empty areas are not shown. Entries outside the internal SSD (M.2,
extended storage, USB drive): if any are selected, there is a separate
confirmation. Files on a USB drive can only be selected individually.
The full cleanup asks for the M.2 and the extended storage whether they
should be wiped too.


IMPORTANT
---------
- NO "Rebuild Database" (see above). Whatever PS5 Cleanup had to leave
  unchanged is named at the end with the reason.
- Console suddenly turns itself off? Some modified PS4 packages (e.g.
  updates with DLC built in afterwards) contain broken add-on entries.
  When the PS5 reads them in the background, it crashes and turns off -
  even if the game is not running. PS5 Cleanup detects such games while
  scanning and shows "WARNING: broken add-on entries". Fix: delete this
  game with PS5 Cleanup (the entries are repaired first) and install it
  again with an unmodified update.
- Deleted data is gone for good. So is saved data without a cloud backup.
- Nothing is deleted until you confirm on the screen "This will be
  deleted" (hold Cross for 1.5 seconds or type DELETE) - and never in
  test mode. The safe choice is preselected in every question. If a size
  has not been counted yet, the program counts it first - NOTHING is
  deleted while it does. "Back to selection" = nothing happens.
- After a full cleanup you have to set up your jailbreak tools again.
  The jailbreak entry points (e.g. YouTube/Y2JB) are never part of the
  full cleanup - they only go if you tick them individually.
- If the program reports an error while scanning (e.g. "could not be read
  completely"), nothing is deleted - just scan again. Better to cancel
  than to delete with an incomplete list.
- If an M.2 is installed but the program reports "no M.2 found": do not
  delete, restart the PS5 instead.
- Use at your own risk. Errors are possible. Back up
  anything important to you first.
- If there are problems: Options, "Log & feedback" shows the log. Please
  send it to us
  (see "FEEDBACK" and "Log" under DETAILS).


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

ShadowMount games
  The menu only has a tile; the game itself is on the M.2 or the internal
  SSD. If the game is deleted, the tile always goes with it. If only the
  tile is deleted, it comes back once ShadowMount is loaded again.
  Before deleting, the program stops ShadowMount+ (1.7 or newer) via its
  stop file - otherwise it would recreate the tiles right away. Deleting
  only goes ahead if ShadowMount confirms this or is provably not running
  (no games mounted). If the state is unclear, you are asked, with
  "Cancel" preselected - if ShadowMount is still running, mounted game
  data could be damaged. Afterwards load ShadowMount again. While the
  Game Compressor is working, the affected game is locked.

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
  the game, never-synced ones are gone. The full cleanup does not delete
  trophy lists of games that stay (e.g. on a kept M.2).

Assignment (which data belongs to which tile?)
  Only what is clearly proven goes under a tile: folders with its ID,
  folders with its name (ignoring case, spaces and hyphens, "+" counts as
  "plus"), a file like xyz_startup.log next to its folder xyz - and
  whatever its program names itself. For this PS5 Cleanup reads the
  program files of the tiles (app folders, homebrew folders, payloads);
  nothing is changed. If two tiles fit equally well or a program could
  not be read completely, nothing is guessed: the entry stays under
  Other. ELF files are never assigned via another app's program - a
  program often names other tools it only uses.

Jailbreak protection
  The entry points your jailbreak starts from are detected and never
  ticked automatically - neither by selecting the whole area nor by the
  full cleanup. Ticking or deleting them shows a warning. Detected:
  YouTube (Y2JB), Netflix ('N Hack), Lua jailbreak (the known Lua games
  and their saves), PS2 save jailbreak (Okage, Star Wars Racer Revenge,
  Jak X), Ren'Py jailbreak (yarpe), WebKit Autoloader (tile and the
  browser's offline storage, also PSH5/Goldengames) and the browser
  storage. Only what the jailbreak needs to start is protected. Jailbreak
  tools (etaHEN, Payload Manager, autoloader settings, update packages,
  ELF files) stay selectable as usual.

Test mode
  At start, test mode is on and "TEST MODE" is shown at the top. The app
  shows everything like real deleting, but the delete buttons are locked -
  if you press them, the app tells you that test mode is on. To switch it
  off: Options, "Test mode". The app does not remember this.

Stopping a deletion
  On the "Deleting" screen, holding Circle stops deleting after the
  current entry. An entry is never cut off halfway. All others stay
  unchanged and are listed in the log. PS5 Cleanup then does not remove
  itself, so you can continue later.

Log
  Options shows the log on your TV at any time. As soon as something is
  deleted, PS5 Cleanup also saves it as a file
  (ps5cleanup_protokoll_<date>_<time>.txt): on the USB drive; if there is
  none, or it is full or not writable, on the internal storage in the
  folder /data/PS5-Cleanup-Protokolle; if that is full too, on the M.2 in
  the folder PS5-Cleanup-Protokolle. Every run gets its own file. The
  folders stay even when PS5 Cleanup removes itself at the end, and are
  never offered for deletion. Only the full cleanup removes old logs on
  the internal storage - never the current one, never those on the USB
  drive or the M.2. Fetch it e.g. over FTP. In the log view, Square
  saves the log to the USB drive even without deleting anything, also in
  test mode.

Privacy - what PS5 Cleanup keeps and sends
  On the PS5, PS5 Cleanup keeps nothing except its own tile and its logs
  (USB drive, otherwise on the PS5) - no settings, no lists, nothing
  remembered between two starts.
  - Update notice: at start, PS5 Cleanup asks GitHub (api.github.com)
    once for the latest version number. Only this request is sent - as
    when opening any website, GitHub sees the internet address. Nothing
    is downloaded or run, only a version number is accepted. Without
    internet nothing happens.
  - Browser control: the page comes from the PS5 itself and only answers
    devices in the home network. Nothing goes to the internet. Nothing
    stays in the browser except the tile images in its cache (10 minutes
    at most).
  - Feedback via QR code: see below - the PS5 sends nothing.

Feedback via QR code (optional)
  The PS5 sends nothing - it only shows a QR code. It contains a link
  with:
  - PS5 Cleanup version, normal or test version, build stamp
  - model number (e.g. CFI-7021), firmware, disc drive pairing status
  - jailbreak entry points set up (e.g. YouTube), connected storage (M.2,
    extended storage, USB drive), ShadowMount
  - numbers: entries per area, unreadable folders, memory use, scan
    duration, selected entries, errors
  - for errors, up to 5 short codes: step, kind of entry, location and
    error code (e.g. "Sony uninstall, PS4 game, internal, 0x80990001")
  No game or app names, no title IDs, no account, no user ID, no files or
  paths, no IP address of the PS5. The key details are shown in plain
  text next to the QR code, everything again on your phone. Nothing is
  stored until you tap "Send" there. Personal details someone types into
  the text field (IP or e-mail addresses, long numbers) are removed
  before storing. As with any website visit, the server briefly sees
  your phone's internet address - it is not stored. The page runs on
  Cloudflare: https://feedback.ps5cleanup.workers.dev (privacy notice
  there under /datenschutz).

Language
  The language follows the PS5 system language (German -> German,
  otherwise English). Before the first scan, Square switches it.

Files
  ps5cleanup_v1.0.9.elf        the PS5 app (starts in test mode)
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

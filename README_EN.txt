PS5 CLEANUP 1.0.7 by da_Chris86
===============================

Brings a jailbroken PS5 back to a freshly-set-up state - without
reinstalling and without a reset. The program never touches your account,
licenses, system software or the disc drive pairing. For all PS5 models
(PS5, Slim, Pro, each with disc drive or as Digital Edition).

WHICH FILE IS WHICH? - PLEASE READ FIRST!
-----------------------------------------
  ps5cleanup_v1.0.7.elf ......... PS5 app. DELETES - but only after you
                                  explicitly confirm!
  PS5-Cleanup-Test_v1.0.7.elf ... PS5 app for trying it out safely (e.g.
                                  on a new firmware). Shows everything,
                                  "TEST" at the top. TECHNICALLY CANNOT
                                  DELETE ANYTHING!
  Not sure, other model or other firmware? Start with the test file!
  (separate download on GitHub)
  From 1.0.7 on there is no Windows version (PS5-Cleanup.exe) any more -
  see "WHY NO WINDOWS VERSION ANY MORE?" below.

IMPORTANT: NO MORE "REBUILD DATABASE"
-------------------------------------
- Sony's "Rebuild Database" (Safe Mode) deletes installed PS4 games from
  fake PKGs on the internal SSD and on the extended storage (USB hard
  drive for games).
  The PKG files stay, but the games have to be installed again. Observed
  on 31 Oct 2026 on a PS5 Pro with firmware 12.00; other tools
  (elf-arsenal, ps5-db-backup) describe the same. According to these
  tools, PS4 games on the M.2 survive - not verified by PS5 Cleanup.
- Earlier versions of this guide recommended the rebuild. Please do not
  do it any more. From 1.0.7 on it is not needed either: the PS5 app
  removes apps, games, screenshots, videos and saved data without leaving
  an empty tile or an empty entry behind. Whatever cannot be removed
  cleanly stays unchanged and is named at the end. If an empty tile
  still stays, please send us the log (see "FEEDBACK") so we can fix it.
- NEVER choose "Reset PS5" or "Reinstall System Software".

TWO WAYS
  - Targeted: tick single entries - e.g. when one game or tool causes
    problems, delete only its data, saved data or trophies. Everything
    else stays. Button "Delete selection only".
  - Full: everything on the internal SSD at once, like freshly set up
    (the M.2 is asked separately). Button "Full cleanup".

TESTED on a PS5 Pro (CFI-7021) with firmware 12.00 and an M.2.
Other models and firmwares are not blocked but have not been tested yet -
there, please start PS5-Cleanup-Test_v1.0.7.elf first and check that
the list is right.

FEEDBACK
  Easiest via QR code: at the end and in the log view (Options), PS5
  Cleanup shows a QR code. Scan it with your phone, tap "It worked" or
  "Problem", done - no account and no PC needed. What is sent is
  described under "Feedback via QR code" in DETAILS. For more detail use
  GitHub (issues or discussions) or psxtools.de - ideally with model,
  firmware and the log (see "Log" under DETAILS). This also works without
  deleting anything.


HOW TO USE (PS5 app, directly on the PS5)
-----------------------------------------
1. Load your jailbreak and start ps5cleanup_v1.0.7.elf with your payload
   loader or Payload Manager. No USB drive needed - if one is plugged in,
   the log is saved there.
2. On first start the "PS5 Cleanup" tile appears. The interface starts
   right away: use the controller (Cross = select, hold Circle = quit,
   Options = log).
3. "Scan", then tick entries (Square = delete selection only) or
   Triangle = full cleanup.
4. At the end: Cross = restart (recommended, preselected) or
   Circle = quit. After the restart, start your jailbreak as usual.
   Do not rebuild the database (see above).


WHY NO WINDOWS VERSION ANY MORE?
--------------------------------
- The Windows version works over FTP. Over FTP, tiles and the entries in
  the Media Gallery and the saved data list cannot be removed cleanly -
  and that is exactly what 1.0.7 is about. It could only have done half
  the job: empty tiles and empty entries would have stayed.
- Anyone who can start an FTP server on the PS5 can just as well start
  the PS5 app there. It can do everything the Windows version could, and
  more.
- Planned for the next version: control PS5 Cleanup from your PC or phone
  in the browser, as many PS5 tools offer by now (e.g. ShadowMount or the
  Payload Manager).
- The source code of the previous Windows version (1.0.6) stays on GitHub
  in the folder Quellcode, but is no longer developed.


NEW IN 1.0.7
------------
PS5 app - everything in one go, without empty tiles, without empty
entries and without a database rebuild. Tested from 1 to 3 Nov 2026 on a
PS5 Pro (CFI-7021) with firmware 12.00, M.2, with and without USB drive:
- Apps, tool tiles and games are removed by Sony itself, like deleting
  them in the PS5 menu - including PS4 games from fake PKGs and
  ShadowMount tiles. An app's data goes with its tile. If a tile stays,
  its data stays too (nothing is half deleted).
- Games that cannot be deleted in the PS5 menu or under "Storage": PS5
  Cleanup first repairs broken add-on information (e.g. from a modified
  patch) itself, then Sony removes the game cleanly. Tested with Red
  Dead Redemption 2, which could not be deleted at all before.
- PS5 Cleanup already detects such games while scanning: the line says
  "WARNING: broken add-on entries", with an explanation and a line in the
  log (see IMPORTANT).
- New: optional feedback via QR code - at the end and in the log view
  (Options). No account and no PC needed, no personal data (see
  "Feedback via QR code" under DETAILS).
- Every app and every tool is shown as one block: the tile at the top,
  below it everything that belongs to it - data and settings folders,
  files on the M.2 and the USB drive, ELF and PKG files. New: PS5 Cleanup
  also looks into the app's own program. If it names a folder or a file
  (e.g. /data/PCSX2), that belongs to this app. This also works for tools
  PS5 Cleanup does not know. Without solid evidence an entry stays under
  Other - nothing is ever guessed (see "Assignment" under DETAILS).
- Tiles without an app folder that only point to a tool on the console
  (e.g. Spectrum Library) are now shown as a tool tile under Apps instead
  of as a leftover under Other.
- Data folders of tools that contain your own games, saves or backups
  (e.g. folders roms, saves, keys, backups or an emulator's memory
  cards) are shown in red, with a note on what was found. "Select the
  whole area" does not tick them.
- Trophies: one block per game - the trophy list at the top, below it
  the progress of all accounts (see "Trophies" under DETAILS).
- Names for leftovers too: saved data, trophies, licenses and captures
  carry the name of their game or app - even if the game is already
  deleted. The name only comes from information on the console itself.
  Without evidence it says "game/app unknown".
- Data that an installed game stores under a different ID (e.g. GTA V)
  is shown with the game's name and is not offered as a leftover.
- Games stored as folders on the M.2: kstuff mounts them again as soon as
  ShadowMount is stopped. PS5 Cleanup releases this mount - only if it
  points exactly to the tile's own game folder -, then Sony removes the
  tile. The game folder stays if you only tick the tile.
- Screenshots and videos are deleted by Sony's own delete service, like
  "Delete" in the Media Gallery. The Media Gallery is clean afterwards,
  without empty entries.
- Saved data goes together with its entry in Sony's saved data list. If
  the game is running, the saved data stays.
- Trophies of PS4 games are assigned to the right game.
- Leftovers without a tile (app folders no longer listed in the PS5
  menu) are deleted directly. Even after "tile only", no empty mount
  folder of the game is left behind.
- Installed PS4 games you did not select are protected. Your jailbreak
  entry points (e.g. YouTube/Y2JB, WebKit Autoloader) stay unless you
  tick them individually.
- Whatever cannot be removed cleanly stays unchanged and is named at the
  end with the reason. If an empty tile still stays, please send us the
  log.
- No USB drive needed, no more database backups. The log is saved on
  the USB drive if one is plugged in. If there is none, or it is full or
  not writable, it goes to the internal storage
  (/data/PS5-Cleanup-Protokolle); if that is full too, to the M.2
  (PS5-Cleanup-Protokolle). Every run gets its own file. It stays even
  when PS5 Cleanup removes itself at the end.
- The first window of the full cleanup shows everything: internal SSD,
  M.2, extended storage (USB hard drive for games) and what stays in any
  case. Games and apps are counted, not lines. Old logs on the internal
  storage are removed with it; those on the USB drive and the M.2 stay.
- "USB drive" means a USB stick or a USB disk with files. The extended
  storage (USB hard drive for games) keeps its name.
- At the end: Cross = restart (recommended) or Circle = quit. There is no
  "Turn off" any more.
- New interface: 7 areas (Games, Apps, Saved data, Trophies, Screenshots
  & videos, Other, Jailbreak), tile images in the list, apps grouped like
  games (tile, below it its data and its ELF file on the USB drive - each
  line can be ticked on its own), for every entry "What is it? / What is
  deleted? / What happens then?", no more cut-off texts.
- Updates and add-ons are shown indented right below their game. In the
  Jailbreak area, saved data, add-on info and app data are shown right
  below their tile (e.g. YouTube).
- No Windows version any more (see "WHY NO WINDOWS VERSION ANY MORE?").


UPDATE 30 Oct 2026 - "1.0.6 Fix"
--------------------------------
PS5 app:
- Much less memory: until now usage grew with every ELF file on the
  console and the USB stick - with many files the app could crash. Now
  it stays small.
- Scanning is faster (an unneeded second scan is gone).
- Tools with private or very deeply nested folders (e.g. Playden) no
  longer stop the scan or lock anything - those folders are skipped and
  never touched.
- Crash reporter: if PS5 Cleanup crashes, a notification shows the
  reason. Please send a photo of it.
- Built for firmware up to 14.10.
- New: PS5-Cleanup-Test.elf for trying it out safely (deletes nothing).
PS5 app and Windows:
- (The database rebuild steps of that time are outdated - see
  "IMPORTANT: NO MORE REBUILD DATABASE" above.)
- The WebKit jailbreak tile "JAILBREAK ME" (WKAG00001) is now also
  detected and protected as a jailbreak.
The program shows "1.0.6 Fix" at the top.


NEW IN 1.0.6
------------
- PS5 Cleanup now runs directly on the PS5 (ps5cleanup.elf), controlled
  with the DualSense on your TV. On first start the ELF adds its own
  "PS5 Cleanup" tile to the home screen. No Homebrew Launcher or websrv
  needed.
- Scanning and ticking deletes nothing. Nothing is deleted until you
  explicitly confirm at the end - you can cancel at any time before.
- PS5 app: tiles (games, apps, tool tiles) are removed through Sony's own
  uninstall service - no empty tiles are left. If you only delete games
  and apps, no database rebuild is needed. If saved data, screenshots or
  videos were deleted, 1.0.6 showed "rebuild the database" at the end -
  this is no longer recommended (see above).
  If Sony refuses, files are deleted as before and the entry is reported
  as PARTIAL (with a list of what was removed and what is left).
- PS5 app: at the end the PS5 is always turned off completely (not rest
  mode) - button "Turn off PS5", no time pressure, with X or O. PS5 Cleanup removes
  itself first (always with the full cleanup, optionally with a
  selection). (From 1.0.7 on: restart or quit.)
- Jailbreak protection: the entry points your jailbreak starts from are
  detected and never ticked automatically - neither by selecting the
  whole area nor by the full cleanup (which asks; "Keep" is recommended).
  Ticking or deleting them shows a warning. Detected: YouTube (Y2JB),
  Netflix ('N Hack), Lua jailbreak (the known Lua games and their saves),
  PS2 save jailbreak (Okage, Star Wars Racer Revenge, Jak X), Ren'Py
  jailbreak (yarpe), WebKit Autoloader (tile and the browser's offline
  storage, also PSH5/Goldengames) and the browser storage. Only what the
  jailbreak needs to start is protected. Jailbreak tools (etaHEN, Payload
  Manager, autoloader settings, update packages, ELF files) stay
  selectable as usual.
- The console overview shows which jailbreak entry points are set up (and
  a Blu-ray jailbreak disc in the drive) and which entries you must not
  delete to keep them.
- Tiles without an app folder (e.g. Spectrum Library) cannot be removed by
  Sony: they are reported as PARTIAL and listed by name at the end; they
  stay in the menu.
- 6 areas (see "WHAT CAN GO"), the location is shown in front of each entry.
- USB storage like M.2: games on USB can be deleted too. The full cleanup
  asks separately for M.2 and USB storage ("keep" is preselected).
  Normal USB sticks only individually.
- Game images (exFAT etc.) count as games everywhere.
- Shorter tags, long names scroll in the PS5 app.
- App data (e.g. the Y2JB file) only goes together with its app.
- The Windows version is now called 1.0.6 (no longer "Beta"). It still
  removes tiles via files - empty tiles can stay in the menu (NO database
  rebuild, see above).


NEW IN 1.0.5 (Windows version)
------------------------------
- ShadowMount handled even more carefully: deleting only goes ahead if
  ShadowMount confirms it stopped or is provably not running (no games
  mounted). Otherwise you are asked - "Cancel" is preselected.
- If the PS5 denies access while reading a folder, the program stops
  instead of treating the folder as empty.
- Folder listings and files only count as complete when the PS5
  explicitly confirms the transfer.
- Test mode: the text now mentions that the model query briefly creates
  its own file /data/ps5info.txt and removes it again.
- Disc drive display fixed: if the PS5 gives no valid answer, it now says
  "not readable" instead of wrongly saying "paired".
- Original PS5 (CFI-10xx/11xx/12xx): always shows "built-in drive" or
  "no disc drive (Digital Edition)", whatever the driver reports.
- Disc drive status 6: neutral text, because its exact meaning is not
  clear yet.
- New note before deleting when a USB drive is connected or the M.2 is
  empty or not detected - otherwise licenses and tiles of those games
  could count as leftovers.
- ps5info.elf rebuilt.
- New interface: console, disc drive pairing (with status legend) and
  storage at a glance, colored notes (green = protected, blue = info,
  yellow = note, red = permanent). Adapts to smaller screens and scales
  up in large or maximized windows. The program remembers window size and
  position. Long names in the list are shown in full when you hover over
  them with the mouse.
- Log: can be collapsed, its height dragged with the divider, "Copy all"
  (always the whole content) and "Open in window".
- The button "COMPLETE - like freshly installed" is now called "Full
  cleanup". The process and the DELETE confirmation are unchanged.
- New: "Send Feedback / Compatibility Report" - a voluntary report (model,
  firmware, disc drive status with raw value, FTP server, test mode
  result, error messages). Anything not detected says "Unknown". IP addresses, Windows
  user name and paths are removed first. You see and can edit the whole
  text; only "Open on GitHub" opens your browser with a pre-filled issue,
  and it is only submitted if you submit it there yourself. GitHub issues
  are public, and you normally need a GitHub account to submit.
- Separate from that and unchanged: at startup the program asks GitHub
  once whether a newer version exists. Only the usual connection data is
  sent, no information about you or your PS5.
- "Save log": note to check the file for personal data before sharing.
- Connecting now also works with FTP servers that use a different kind of
  data connection (PASV instead of EPSV). If it still fails, the program
  says more precisely why - the reason is written to the log.
- "Find PS5 on network" now finds FTP servers on port 2121 and 2120
  (zftpd uses 2120). If there are several, you pick one from a list. If you
  enter only the IP, the program tries both ports by itself.
- The log names the FTP server and its version - only what the server says
  about itself, otherwise "Unknown" - and whether the data connection used
  EPSV or PASV. Both are also in the feedback report.
- Simpler error messages; technical details are in the log.
- While deleting, a red note shows that deleting is in progress, and the
  progress shows as "entry X of Y". What is already deleted stays deleted -
  cancelling or an error does not bring anything back.
- New logo.


NEW IN 1.0.4 (Windows version)
------------------------------
- A real Windows program (previously a packaged PowerShell script) -
  same handling, same protection rules.
- English and German: the language follows Windows; the button at the
  top right switches it (the program restarts briefly).
- M.2: every game and every file in the M.2 folders is listed
  individually. The folders themselves stay.
- The list shows the size of every entry and every section. Counting
  starts in the background right after the scan - the long count before
  deleting is mostly gone.
- "Cancel" button while searching and counting.
- A notice when a newer version is available on GitHub.


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
- NO "Rebuild Database" - it deletes installed PS4 games (fake PKG) on the
  internal SSD and the extended storage (see above). Whatever the PS5 app
  had to leave unchanged is named at the end with the reason.
  NEVER choose "Reset PS5" or "Reinstall System Software"!
- Console suddenly turns itself off? Some modified PS4 packages (e.g.
  updates with DLC built in afterwards) contain broken add-on entries.
  When the PS5 reads them in the background, it crashes and turns off -
  even if the game is not running. PS5 Cleanup detects such games while
  scanning and shows "WARNING: broken add-on entries". Fix: delete this
  game with PS5 Cleanup (the entries are repaired first) and install it
  again with an unmodified update.
- Deleted data is gone for good. So is saved data without a cloud backup.
- Nothing is deleted until you agree in the final question. The safe
  choice is preselected in every question. If a size has not been counted yet, the
  program counts it before the final question - NOTHING is deleted while
  it does. "Cancel" = nothing happens.
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
- If there are problems: Options shows the log. Please send it to us
  (see "FEEDBACK" and "Log" under DETAILS).


DETAILS (only if you want to know exactly)
------------------------------------------

Disc drive pairing
  On Slim and Pro, the detachable disc drive is paired with the console.
  The program reads and shows the pairing status. It never touches system
  software or system settings. The original PS5 has a built-in drive
  without pairing, Digital Editions have none.
  The display tells apart: "Pairing confirmed", "No pairing", "not
  reliably determinable" (a status value whose meaning is not confirmed)
  and "not readable" (no valid answer).

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

Log
  Options shows the log on your TV at any time. As soon as something is
  deleted, ps5cleanup_v1.0.7.elf also saves it as a file
  (ps5cleanup_protokoll_<date>_<time>.txt): on the USB drive; if there is
  none, or it is full or not writable, on the internal storage in the
  folder /data/PS5-Cleanup-Protokolle; if that is full too, on the M.2 in
  the folder PS5-Cleanup-Protokolle. Every run gets its own file. The
  folders stay even when PS5 Cleanup removes itself at the end, and are
  never offered for deletion. Only the full cleanup removes old logs on
  the internal storage - never the current one, never those on the USB
  drive or the M.2. Fetch it e.g. over FTP. In the log view, Square
  saves the log to the USB drive even without deleting anything. The
  test file (PS5-Cleanup-Test_v1.0.7.elf) saves nothing - just take a
  photo of the log view there.

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
  ps5cleanup_v1.0.7.elf        the PS5 app
  PS5-Cleanup-Test_v1.0.7.elf  the PS5 app for trying it out - deletes nothing
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

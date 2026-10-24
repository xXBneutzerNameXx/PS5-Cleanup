PS5 CLEANUP 1.0.5 BETA by da_Chris86
====================================

Brings a jailbroken PS5 back to a freshly-set-up state - without
reinstalling and without a reset. The program never touches your account,
licenses, system software or the disc drive pairing. For all PS5 models
(PS5, Slim, Pro, each with disc drive or as Digital Edition).

TWO WAYS
  - Targeted: tick single entries - e.g. when one game or tool causes
    problems, delete only its data, saved data or trophies. Everything
    else stays. Button "Delete selection only".
  - Full: everything on the internal SSD at once, like freshly set up
    (the M.2 is asked separately). Button "Full cleanup".

BETA: Tested on a PS5 Pro (CFI-7021) with firmware 12.00 and an M.2.
Other models and firmwares should work but have not been tested yet.
Feedback is easiest with the button "Send Feedback / Compatibility
Report" - even without deleting anything. Nothing is sent automatically.


NEW IN 1.0.5
------------
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
  yellow = note, red = permanent). Adapts to smaller screens; the log can
  be collapsed and expanded.
- The button "COMPLETE - like freshly installed" is now called "Full
  cleanup". The process and the DELETE confirmation are unchanged.
- New: "Send Feedback / Compatibility Report" - a voluntary report (model,
  firmware, disc drive status with raw value, test mode result, error
  messages). Anything not detected says "Unknown". IP addresses, Windows
  user name and paths are removed first. You see and can edit the whole
  text; only "Open on GitHub" opens your browser with a pre-filled issue,
  and it is only submitted if you submit it there yourself. GitHub issues
  are public, and you normally need a GitHub account to submit.
- Separate from that and unchanged: at startup the program asks GitHub
  once whether a newer version exists. Only the usual connection data is
  sent, no information about you or your PS5.
- "Save log": note to check the file for personal data before sharing.
- New logo.


NEW IN 1.0.4
------------
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


QUICK START
-----------
1. Turn on the PS5 and load the jailbreak. The FTP server must be running
   (port 2121) - recommended: ftpsrv by drakmor, tested with version 1.16.
2. Start PS5-Cleanup.exe - one file, Windows 10/11, nothing to install.
3. "Find PS5 on network", then "Connect and scan".
   The program starts in test mode: it only shows what could go, the
   delete buttons are locked. Nothing is deleted - only the model query
   briefly creates its own file /data/ps5info.txt and removes it again.
4. To delete, untick "Test mode". Then either
   "Full cleanup"  (everything on the internal SSD,
   the M.2 is asked separately, type DELETE to confirm)
   or  tick entries and  "Delete selection only".
5. Afterwards on the PS5: rebuild the database (see IMPORTANT).


THE PROGRAM NEVER TOUCHES
-------------------------
- System software and disc drive pairing
- User account, activation and real licenses
- Sony system apps and system settings


WHAT CAN GO (you choose)
------------------------
[1] Games & apps on the internal SSD - game, update and every DLC can be
    selected individually (the game takes its update and DLC with it)
[2] Saved data
[3] Trophies
[4] Screenshots & videos
[5] Downloads, add-ons & leftovers (caches, error reports, empty tiles)
[6] Jailbreak tools & homebrew data (set up your jailbreak again afterwards)
[7] M.2 SSD contents (only if one is installed) - every game and file
    individually; if M.2 entries are selected, there is a separate
    confirmation


IMPORTANT
---------
- Rebuild the database, otherwise empty tiles stay in the menu:
  turn off the PS5, hold the power button until the second beep,
  connect the controller via USB, choose "Rebuild Database".
  NEVER choose "Reset PS5" or "Reinstall System Software"!
- Deleted data is gone for good. So is saved data without a cloud backup.
- Nothing is deleted until you agree in the final question. The safe
  choice is preselected in every question. If a size has not been counted yet, the
  program counts it before the final question - NOTHING is deleted while
  it does. Closing the window or "Cancel" = nothing happens.
- After a full cleanup you have to set up your jailbreak again.
- If the program reports an error while scanning (e.g. "could not be read
  completely"), nothing is deleted - just connect again. Better to cancel
  than to delete with an incomplete list.
- If connecting or scanning does not work at all: use the FTP server
  ftpsrv by drakmor (tested with 1.16).
- If an M.2 is installed but the program reports "no M.2 found": do not
  delete, restart the PS5 instead.
- Use at your own risk. This is a beta - errors are possible. Back up
  anything important to you first.
- If there are problems: the program writes everything to PS5-Cleanup.log
  (next to the .exe, otherwise in %TEMP%\PS5-Cleanup). "Save log" also
  saves the file directly - please attach it when reporting a problem.
- Antivirus: the .exe is not digitally signed. Some antivirus scanners
  wrongly flag new, unsigned programs as suspicious. The complete source
  code is included - if you like, build the .exe yourself
  (Quellcode\bauen.cmd, needs only Windows).


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
  background over a second connection and shows it. "min." means: not
  fully counted (very large or not readable).

Trophies
  Section 3 deletes the trophy data on the console; trophies synced with
  PSN come back the next time you start the game, never-synced ones are
  gone. With a full cleanup, the trophy lists (descriptions) of games that are
  still installed are only deleted if the M.2 is wiped too.

Tile info
  For every deleted game, the program also removes the PS5's tile info
  (/system_data/priv/appmeta). Otherwise the menu shows an empty tile with
  a lock icon after the database rebuild.

Model and firmware
  The firmware is read via FTP. For model and disc drive status the program
  (only with "Detect model automatically" ticked) briefly runs ps5info.elf
  on the PS5 - via the payload loader (port 9021) or the Payload Manager
  (port 8084) - and removes it afterwards. ps5info only reads. It briefly
  stores its answer in its own file /data/ps5info.txt, which the program
  removes afterwards. If neither works, you can type in the model number
  or skip it.

New version
  At start the program asks GitHub once which version is the newest. If it
  is newer, a notice with a link appears at the top. Nothing is downloaded
  and nothing about you is sent.

Language
  The language follows Windows (German Windows -> German, otherwise
  English). The button at the top right switches it; the choice is saved
  in %APPDATA%\PS5-Cleanup\sprache.txt.

Files
  PS5-Cleanup.exe           the program - a single file, nothing else is
                            needed (model query and this guide are built in)
  README_EN.txt             this guide (English)
  LIESMICH.txt              this guide (German)
  LICENSE.txt               license (MIT) and components by other authors
  Quellcode\                for anyone who wants to read or build it:
    *.cs, Fenster.xaml      the program (C#, .NET Framework 4.8)
    bauen.cmd               builds PS5-Cleanup.exe with the compiler that
                            comes with Windows
    ps5info.elf / ps5info.c model and disc drive query for the PS5
                            (GPL-3.0, build instructions in ps5info.mk)
    powershell-1.0.3\       the previous PowerShell version (1.0.3) incl.
                            the text version PS5_Cleanup.bat


LEGAL
-----
PS5 Cleanup is an unofficial, independent community project by da_Chris86.
It is not affiliated with, endorsed by, or sponsored by Sony Interactive
Entertainment. PlayStation and PS5 are trademarks of Sony Interactive
Entertainment Inc.

License: MIT - Copyright (c) 2026 da_Chris86. Anyone may use, share and
modify the program as long as the author and the license text are kept
(see LICENSE.txt). The model query ps5info is licensed under GPL-3.0 -
details in LICENSE.txt.

Made with the help of the AI Claude (Anthropic). Idea, requirements and
the tests on the real console come from da_Chris86.

# PC display settings

PC settings extend original Options > Screen / ScreenRoot, reusing its frame,
fonts, dropdowns, cursor and Cancel/OK controls. Rows: Aspect Ratio (4:3 / 16:9),
Window Mode, Resolution, UI Filter, Text Clarity, VSync and Upscaling.

Display ratio is independent of window resolution. Clipping and viewports use
the same fitted transform. Widescreen expands mesh horizontal field of view,
including reflection camera reconstruction, rather than stretching the car.
UI uses uniform scaling: fonts and controls remain proportional and centered;
full-width backgrounds extend into the additional horizontal space. Console page-specific
widescreen adjustments have not been fully compared.

UI filtering and text clarity preview immediately. Cancel restores committed
quality without saving. OK applies and saves PC preferences under SDL's
OpenGT4 preference directory in video.ini, independent of game save data.
Custom resolutions remain available. aspect_ratio=0 means 4:3, 1 means 16:9.

FSR reconstructs the MSAA-resolved scene using EASU then RCAS. Modes: native,
ultra quality (1.3), quality (1.5), balanced (1.7), performance (2.0).
Model textures and UI atlases are no longer enlarged individually by FSR.
UI draws at final drawable resolution. Original bitmap detail remains limited.

Validation: Release build, menu_tests, ui_text_tests and run_video_smoke.py with
a user-owned USA v2.00 disc. Smoke checks original settings entry, dropdown
cancel, save/reload, cancellation without writes, custom resolution, aspect
save/reload/cancel, identical UI-only output with FSR on/off, and five scene
resolutions. Captures: build/ui_quality.

Confirmations now render the original /projects/cursor ConfirmDialog above
the current menu. Title/message and button labels come from the original
script call. Arrow keys switch buttons; Enter accepts; Escape cancels; the
underlying VM waits without reentering. OS message boxes have been removed
from menu confirmations and race preview notices. --dialog-answer remains a
deterministic test override; --dialog-shot captures the original modal.


SliderBar now renders authored closed fields and expanded sliders, reads typed
range values, clamps keyboard edits, and restores an expanded edit on Escape.
Picture Quality displays all eight controls. Label fitting reserves a gutter.
Options scrolling keeps fitting axes stationary; changing focus to Sound no
longer shifts the entire grid left. Returning to a cached menu resets its old
exit fade, including repeated Options exits. Selection, adjustment and modal
confirmation dispatch native system-bank SFX.

Original game.option preferences commit through setNeedSave and reload from
video.ini.game_options; cancelling a page does not commit its draft. Menu SFX
volume now controls system-bank playback. The stereo damped menu reverb is a
provisional native approximation, not a measured match to the PS2 preset.

Remaining backend work: Picture Quality values save but race/replay colour
postprocessing is not connected; several network, controller-feedback and
photo operations have no native backend. Language availability currently
reflects the supplied USA project only. Do not describe all Options as fully
functional. Updated Release executable: build/Release/opengt4.exe. A separate
opengt4_preview.exe is also available for isolated checks.


Miscellaneous: all four original controls commit and reload; resetMovieCount
now resets the shared demonstration schedule so apply can reach Auto Save.
Opening Movie Off skips the opening during normal startup (legal notices remain;
explicit --intro still overrides this for validation). Auto Demo uses the
original script's inactivity timer, with a native default of 30 seconds and
10-second UI steps. Integer toFloat now works in the Adhoc host bridge.
The movie stage returns through boot/MovieRoot; cached entries reset end_flg,
restart the decoder, and accept/cancel return to the menu. Demo Movie Interval
counts attract requests; Off disables the movie stage. Race replay stages are
still absent, so skipped stages stay in the menu until the next idle interval.
Auto Save preferences persist but saving career progress requires a native
career/save engine; this is not implemented by saving an option flag.

The requested cadence is opening on startup, principal menu, then opening
again after the configured Auto Demo inactivity period. Menu input resets that
timer; it does not loop the opening immediately at EOF. Full movie completion
is also checked with --shot-delay 205 using a one-second fixture idle timer;
normal UI values remain multiples of ten seconds. Save-system implementation
is explicitly deferred to the user's next request.

Release validation passed the complete opening without skipping: opening_us
reported EOF after 11750 displayed frames, returned to TopRoot, then restarted
MovieRoot on the idle timer; the second playback had reached 94 frames at the
capture. Use run_video_smoke.py --full-opening for this longer regression.

### Fake86 changelog for v0.14.0.0

v0.14.0.0:
  - __This release is a major work in progress re-write and should not be
    considered stable.__
   
    _In terms of features and compatibility its a step backwards, but a
    necessary one for the long term maintainability and roadmap of the project._

  - Update to [pcxtbios.bin V3.0](http://www.phatcode.net/downloads.php?id=101).
    The source for this is public domain and so has been pulled into the
    repository for quick reference.

  - The original code had a bootstrap hack in to set the video mode and number
    of attached hard drives in the bios area.  This caused a performance problem
    as the hack was in the main read memory call.  Setting up the bios area is
    now handled by a custom option rom and the hack has been removed.
    
  - Fake86 was multi-threaded to try and work around timing issues, however this
    complicated the code a lot.  At first the threading code was made portable
    by using SDL_Thread's.  After a larger rework all of the threading code was
    removed and an entirely new timing model was created for Fake86 based on
    master clock cycles.
    
  - The code has been clang-formatted, a new CMake based build system has been
    integrated and a vast vast amount of code refactoring has been conducted.

  - All additional peripherals have been removed to focus the core code on
    stability and correctness.  In time these will be added back in.
    
  - The PIT timer has been entirely re-written to be more accurate and fully
    featured.

  - The disk handler has been largely re-written to support in memory disk
    loading, as well as direct disk pass through on Windows.
    
  - Total re-write of the command line parser to be more easy to maintain

  - The blit 1x and 2x functions have been re-written to be more performant and
    more concise.
    
  - A simplistic Internal PC Speaker emulator was written, with known
    deficiencies which will be addressed in the future.  It was mostly
    integrated at this stage to prove the PIT timer rewrite and the audio path.
    Future work is to buffer the PIT frequency changes so the audio thread can
    render them cycle accurately.

  - The SDL Event handler has been re-written and is more maintainable.

  - Compilation regularly checked on Linux machine.

  - Common module interfaces have been hoisted out into `common.h` rather then
    being declared `extern` inside each compilation unit for better
    maintainability.
    
  - A dedicated logging system has been introduced and integrated into all of
    the major modules in Fake86.

  - Port handling has been revised to try and improve correctness and
    compatibility.

  - Gearing up for a re-write of the video adapter emulation for better game
    support.

  - Consistent effort to document non trivial code so it can serve as a learning
    tool for others.

  - Code maintainer changed from [Mike Chambers](http://rubbermallet.org/) to
    [Aidan Dodds](github.com/8bitpimp).

v0.13.9.16:
  - Fixed regression that caused Fake86 to not compile on at least some
    versions of Mac OS X.

  - Increased amount of processing time dedicated to timing and audio
    sample generation. Helps accuracy and helps prevent audio dropouts
	on slower systems.

v0.13.9.13:
  - Major fix: Some games using the Sound Blaster used to go silent and some
    even would hang after playing the first sample chunk. It was due to them
    being unable to read the data count register from the DMA controller.
    This has been fixed. One example game that did this was Return to Zork.

  - Numerous big fixes and improvements.

  - Windows version now has a simple drop-down menu GUI, so on that platform
    it is no longer required to specify command-line options for the basics
    such as specifying disk images. These can be loaded from the menus.


v0.12.9.19:
  - Fixed bug from v0.12.9.16 where the call to createscalemap() in the
    VideoThread() function was placed before the mutex lock, sometimes causing
    crashes when Fake86 first loads. (due to an uninitialized pointer)

  - Fixed bug in imagegen that made it crash on some Windows systems.


v0.12.9.16:
  - Fixed bug from v0.12.9.12 that caused faulty CPU emulation on big-endian
    host CPUs, such as the PowerPC.

  - Fixed another bug from v0.12.9.12 that caused colors to be incorrect on
    big-endian host CPUs.

  - Added "-slowsys" command line option that can be used to fix audio dropout
    issues on very slow host CPUs by decreasing audio generation quality.

  - Added another stretching function that is used when the SDL window is
    exactly double of the width and height of the emulated video. Only active
    when no smoothing method has been specified. This saves the CPU a lot of
    work since we now only need to calculate the color to draw once for every
    four pixels, instead of recalculating the same color for every pixel
    This makes difference between smooth video and jerky, useless video on
    very old systems such as my 400 MHz PowerPC G3 iMac.

  - Added an obvious speed tweak which I should have had all along by
    generating a "scale map" lookup table once on every screen mode change.
    we no longer need to make expensive floating point scale calculations on
    every pixel in every frame. The speed boost is very noticeable on slower
    machines such as those with a Pentium 4 or older CPU.

  - No longer distributing IBM's ROM BASIC bundled with Fake86 out of legal
    concern. Is it very old? Yes. Does IBM care? Likely not, but they still
    hold the copyright. You can still use ROM BASIC by providing your own ROM
    dump. It should be 32,768 bytes in size, and must be named "rombasic.bin"
    without the quotes. After building and installing Fake86, you need to put
    the ROM dump in "/usr/share/fake86/" unless you explicitly changed the
    path in the makefile. In the case of running under Windows, the file
    should be placed in the same directory as the executable.

  - Created and included a new makefile, this one customized to work in
    Haiku OS. More information about Haiku at their website:
    http://www.haiku-os.org


v0.12.9.12:
  - Around 80% of this version is a rewrite compared to all older versions of
    v0.11.7.22 and prior, and I am considering it a "fresh start". Older
    releases will no longer be officially available, and are to be considered
    obsolete. This fresh code base has far too many bugfixes, tweaks, design,
    layout, and speed improvements to even bother putting together a list of
    changes.

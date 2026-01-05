# disk_copy: tool to manipulate Apple classic 68k Macintosh 'Disk Copy' images.

The 'Disk Copy' program provided by Apple came in several versions.
This tool (for now) processes only 'Disk Copy 4.2' (`DC42`) format
images.

DART ("Disk Archive/Retrieval Tool") 1.5 (version numbers reached 1.5.3)
produces a compressed image format.

Later versions of Disk Copy supported `NDIF`, DMF PC-format, and "other" image
file formats, as well as images segmented into multiple files.

# Command-line

    disk_copy extract --disk_copy file.dc42 --output_image file.img \
                      [--ignore_data_checksum]

Attempts to extract the data bytes from `file.dc42`, writing them as `file.img`.

Tag data is ignored. If --ignore_data_checksum is specified, any mismatch in
data checksum is ignored. Tag data checksum is not checked in either case.

    disk_copy create --input_image file.img --disk_copy file.dc42

Attempts to encode the contents of `file.img` (assumed to be raw disk image)
into a `DC42`-format file named `file.dc42`

    disk_copy verify --disk_copy file.dc42

Verifies the apparent format, and checksums for data *and tag* sections.
Returns an error status and emits diagnostic messages if the disk_copy
file cannot be validated.

Flag options are supported through the Abseil Flags library, https://abseil.io/docs/cpp/guides/flags,
which provides flags including `--help`, `--helpshort`, and other features.

# Error codes

Diagnostic error messages are printed to standard error.

*  0    : success.
*  1, 2 : invalid command-line arguments.


# Building

This software uses [Bazel](https://bazel.build/) and depends on the Abseil flags library.
The build is configured using `MODULE.bazel` and BUILD.

# More information

https://en.wikipedia.org/wiki/Disk_Copy

DART version history is described at

https://web.archive.org/web/20131219231333/http://support.apple.com/kb/TA29157?viewlocale=en_US

Disk Copy 6.3.1 and changes for versions from 6.1 are described at

https://web.archive.org/web/20110227061252/http://support.apple.com/kb/DL1262

and appears to be downloadable in a MacBinary-encoded SMI "self-mounting image"

https://web.archive.org/web/20150608125306/http://download.info.apple.com/Apple_Support_Area/Apple_Software_Updates/English-North_American/Macintosh/Utilities/Disk_Copy/Disk_Copy_6.3.3.smi.bin

Versions localized for other languages might be available at the DL1262 archive page.

Disk Copy 6.3.2 is described at

https://web.archive.org/web/19990503095942/http://asu.info.apple.com/swupdates.nsf/artnum/n11162

Copies of Disk Copy appear to be available at

https://www.macintoshrepository.org/2416-diskcopy-4-2-5-0-5-5-6-0-6-2-6-3-3-6-4-6-5b13-7-0-8-0

The Gryphel Project (producer of Mini vMac emulator) includes a tool to verify `DC42` checksums.

https://www.gryphel.com/c/minivmac/extras/dc42chk/index.html

DiskCopy 4.2 format is documented in

[Apple II File Type Note](https://web.archive.org/web/20191231202510/http://nulib.com/library/FTN.e00005.htm)

and

https://web.archive.org/web/20201028142058/https://wiki.68kmla.org/DiskCopy_4.2_format_specification



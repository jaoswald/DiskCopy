#ifndef __DISK_COPY_H__
#define __DISK_COPY_H__

#include <cstdint>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"

class DiskCopyChecksum {
 public:
 explicit DiskCopyChecksum(uint32_t initial_sum = 0) :
  sum_(initial_sum) {}

  uint32_t UpdateSum(uint16_t new_word);
  uint32_t Sum() const { return sum_; }

  absl::Status UpdateSumFromFile(std::ifstream& s, uint32_t byte_count);
  
 private:
  uint32_t sum_;
};

class DiskCopyHeader {
 public:
  template <typename Sink>
    friend void AbslStringify(Sink& sink, const DiskCopyHeader& h) {
    absl::Format(&sink, "%s", h.DebugString());
  }

  // Human-readable description of the file header.
  std::string DebugString() const;

  // Read header from a (binary-format) file stream; seeks to the start of
  // the stream, leaving s positioned at the start of the data.
  static absl::StatusOr<DiskCopyHeader>
    ReadFromDisk(std::ifstream& s);

  // Writes header to (binary-format) file stream; it DOES NOT seek the
  // stream before writing.
  absl::Status WriteToDisk(std::ofstream& s);

  // Create a header for an HFS floppy with the specified volume name.
  // Returns an error if the name is too long.
  // data_block_count is the size in HFS (512-byte) disk blocks.
  // Returns an error if data_block_count does not appear to be a 400k, 800k,
  // 720k or 1440k floppy.
  static absl::StatusOr<DiskCopyHeader>
  CreateForHFS(absl::string_view name,
	       uint32_t data_block_count,
	       uint32_t data_checksum,
	       uint32_t tag_byte_count = 0,
	       uint32_t tag_checksum = 0);

  // Verify the data checksum of an image:
  // Read the data words from ifstream s, based on the header contents.
  // Compute the data checksum, and compare it to header_data_checksum_.
  // If the data can be read and the computed checksum matches,
  // return OK; otherwise an error.
  //
  // Note that the file should contain an integer number of 16-bit data words,
  // i.e. the data byte count should be a multiple of 2.
  absl::Status VerifyDataChecksum(std::ifstream& s);

  // Verify the Tag checksum as with VerifyDataChecksum; however, if the
  // header indicates no tag bits are present, always return OK without
  // reading any data.
  absl::Status VerifyTagChecksum(std::ifstream& s);

  // Checks header for validity; if header appears valid, returns the total
  // file size (in bytes) it represents.
  absl::StatusOr<uint32_t> Validate() const;

  // Total file size, in bytes, for the image file described by the header.
  uint32_t TotalFileSize() const;

 private:
  static constexpr size_t kHeaderLength = 84;
  static constexpr size_t kMaxNameLength = 63;
  static constexpr uint16_t kPrivate = 0x100;  // magic number

  explicit DiskCopyHeader(const char header_bytes[kHeaderLength]);
  DiskCopyHeader(const std::string_view name,
		 uint32_t data_size,
		 uint32_t tag_size,
		 uint32_t header_data_checksum,
		 uint32_t header_tag_checksum,
		 uint8_t disk_format,
		 uint8_t format_byte)
    : name_length_(std::min(name.length(), kMaxNameLength)),
      data_size_(data_size),
      tag_size_(tag_size),
      header_data_checksum_(header_data_checksum),
      header_tag_checksum_(header_tag_checksum),
      disk_format_(disk_format),
      format_byte_(format_byte),
      private_(kPrivate) {
    memset(name_bytes_, 0, kMaxNameLength);
    strncpy(name_bytes_, name.data(), name_length_);
  }
  
  size_t name_length_;
  char name_bytes_[kMaxNameLength];

  // On disk, these are stored in classic Macintosh "big-endian" format.
  uint32_t data_size_;
  uint32_t tag_size_;
  uint32_t header_data_checksum_;
  uint32_t header_tag_checksum_;
  
  // Allegedly 0 = 400k  [GCR CLV ssdd] (Mac single-sided)
  //           1 = 800k  [GCR CLV dsdd] (Mac double-sided)
  //           2 = 720k  [MFM CAV dsdd] (PC double-density, double-sided)
  //           3 = 1440k [MFM CAV dshd] (PC high-density)
  //
  // TODO: 68k MLA suggests 'Other encodings may exist, as DC42 was originally
  //                         designed to be able to image HD20 disks.'
  //
  // [CLV = continuous linear velocity; motor speed changes depending on track
  //  CAV = continuous angular velocity; motor speed fixed, recording density
  //        is higher on inner tracks]
  uint8_t disk_format_;

  // Allegedly 0x12 = 400k
  //           0x22 > 400k, Apple II which are not 800k
  //           0x24 = 800k Apple II disk.
  //
  // TODO: 68k MLA suggests this is actually a bit field.
  //
  // For GCR (disk_format_ = 0 or 1)
  //
  // This byte is a copy of the GCR format nybble (6 bits),
  // which appears in the headers of every GCR sector.
  //
  // $02 = Mac 400k
  // $12 = (documentation error claims this is for mac 400k disks, but this is
  //        wrong)
  // $22 = Disk formatted as Mac 800k
  // $24 = Disk formatted as Prodos 800k (AppleIIgs format)
  // $96 = INVALID (Disk was misformatted or had GCR 0-fill (0x96 which
  //       represents data of 0x00) written to its format byte)
  //  Values for bitfield:
  //  76543210
  //  ||||||||
  //  |||\\\\\- These 5 bits are sector interleave factor:
  //  |||            setting of 02 means 2:1 interleave:
  //  |||            0  8  1 9  2 10 3 11 4 12 5  13 6  14 7  15
  //  |||            setting of 04 means 4:1 interleave:
  //  |||            0  4  8 12 1 5  9 13 2 6  10 14 3  7  11 15
  //  ||\------ This bit indicates whether a disk is 2 sided or not.
  //  ||          0 = 1 sided, 1 = 2 sided.
  //  \\------- always 0, as GCR nybbles are only 6 bits
  //
  // For MFM (disk_format_ = 2 or 3)
  // This byte is used to define MFM sector size and whether the disk is
  // two sided or not.
  // Interleave is ALWAYS 1:1 for these formats.
  // $22 = double-sided MFM diskettes with 512 byte sectors
  // Values for bitfield:
  // 76543210
  // ||||||||
  // |||\\\\\- These 5 bits are sector size as a multiple of 256 bytes
  // |||       i.e. 02 = 2*256 = 512 bytes per sector
  // ||\------ This bit indicates whether a disk is 2 sided or not.
  // ||          0 = 1 sided, 1 = 2 sided.
  // \\------- unused, always 0
  //
  uint8_t format_byte_;
  
  // Should always be 0x0100; effectively a magic number.
  uint16_t private_;

  // header is followed by
  //   data_size_ bytes of disk data.
  //   tag_size_ bytes of tag data
  //
  // The tag data is 12 bytes per 512-byte disk sector, and is stored, like the
  // Image data, in sector order. The actual format for each 12-byte block of
  // the Tag data differs for Lisa, MFS and HFS disks, and for MFS or HFS any
  // of them may be wrong or absent! be warned!
  //
  // The Tag format for Lisa 400k or 800k disks is currently unknown, but
  // without tags the disks will not function.
  // For MFS filesystems the Tag format is as follows:
  // BE WARNED: when reading tag data, if the bit at 00 40 00 00 of any of the
  // 3 32 bit words of the tag is set, the tag data for the sector it is part
  // of is trashed and can be ignored. There IS a puprose to the data written
  // when 0x40 is set, I'm just not sure what it is.
  //
  // offset     type/size    contents
  // 0x00       BE_UINT32    File number on disk, within MFS filesystem
  // 0x04       BE_UINT16    Flags bitfield:
  //        FEDCBA98 76543210
  //        |||||||| ||||||||
  //        |||||||| |||\\\\\- unknown, seems unused
  //        |||||||| ||\------ If set, Tag for this sector is not valid.
  //        |||||||| \\------- unknown
  //        |||||||\---------- sector content type: 0: system file;
  //        |||||||                                 1: user file (guessed)
  //        ||||||\----------- sector is part of a: 0: data fork;
  //        ||||||                                  1: resource fork
  //        |\\\\\------------ unknown
  //        \----------------- unknown, sometimes set on the last few sectors
  //                           of a data or resource fork
  // 0x06       BE_UINT16    Logical block number within the file
  // 0x08       BE_UINT32    Time of last modification, in seconds since
  //                         0:00:00, 1/1/1904
  // Note that the last mod time may be different on the final sector of a
  // file; this may indicate something special.
};

#endif // __DISK_COPY_H__

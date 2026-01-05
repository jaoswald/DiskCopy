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
  
 private:
  static constexpr size_t kHeaderLength = 84;
  static constexpr size_t kMaxNameLength = 63;
  static constexpr uint16_t kPrivate = 0x100;  // magic number

  // Checks header for validity; if header appears valid, returns the total file size it represents.
  absl::StatusOr<uint32_t> Validate() const;

  // Total file size, in bytes, for the image file described by the header.
  uint32_t TotalFileSize() const;
  
  uint16_t BigEndian2(const char b[2]) {
    const uint8_t* c = (const uint8_t *) b;
    return c[0] << 8 | c[1];
  }
  uint32_t BigEndian4(const char b[4]) {
    const uint8_t* c = (const uint8_t *) b;
    return c[0] << 24 | c[1] << 16 | c[2] << 8 | c[3];
  }
  
  explicit DiskCopyHeader(const char header_bytes[kHeaderLength]);
    
  size_t name_length_;
  char name_bytes_[kMaxNameLength];

  // On disk, these are stored in classic Macintosh "big-endian" format.
  uint32_t data_size_;
  uint32_t tag_size_;
  uint32_t header_data_checksum_;
  uint32_t header_tag_checksum_;
  
  // Allegedly 0 = 400k
  //           1 = 800k
  //           2 = 720k
  //           3 = 1440k
  uint8_t disk_format_;

  // Allegedly 0x12 = 400k
  //           0x22 > 400k, Apple II which are not 800k
  //           0x24 = 800k Apple II disk.
  uint8_t format_byte_;
  
  // Should always be 0x0100
  uint16_t private_;

  // header is followed by
  //   data_size_ bytes of disk data.
  //   tag_size_ bytes of tag data
};

#endif // __DISK_COPY_H__

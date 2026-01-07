#include "disk_copy.h"

#include <fstream>

#include "absl/strings/str_cat.h"
#include "endian.h"

DiskCopyHeader::DiskCopyHeader(const char header_bytes[kHeaderLength]) {
  name_length_ = header_bytes[0];
  memcpy(name_bytes_, header_bytes+1, kMaxNameLength);
  data_size_ = BigEndian4(header_bytes+64);
  tag_size_ = BigEndian4(header_bytes+68);
  header_data_checksum_ = BigEndian4(header_bytes+72);
  header_tag_checksum_ = BigEndian4(header_bytes+76);
  disk_format_ = header_bytes[80];
  format_byte_ = header_bytes[81];
  private_ = BigEndian2(header_bytes+82);
}

// static
absl::StatusOr<DiskCopyHeader> DiskCopyHeader::ReadFromDisk(std::ifstream& s) {
  s.seekg(0);
  if (s.fail()) {
    return absl::OutOfRangeError("Could not seek to DiskCopyHeader");
  }
  char header_bytes[kHeaderLength];
  if (!s.read(header_bytes, kHeaderLength)) {
    return absl::OutOfRangeError(
       absl::StrCat("Could not read ", kHeaderLength, " bytes"));
  }
  return DiskCopyHeader(header_bytes);
}

namespace {

  absl::StatusOr<std::string> DiskFormatByte(const uint8_t dfb) {
    switch (dfb) {
    case 0:
      return "400k";
    case 1:
      return "800k";
    case 2:
      return "720k";
    case 3:
      return "1440k";
    default:
      return absl::InvalidArgumentError(
	  absl::StrFormat("Unknown Disk Format Byte=%d",
			  dfb));
    }
  }

  absl::StatusOr<std::string> FormatByte(const uint8_t fb) {
    switch(fb) {
    case 0x12:
      return "400k";
    case 0x22:
      return ">400k";
    case 0x24:
      return "800k Apple II";
    default:
      return absl::InvalidArgumentError(
	absl::StrFormat("Unknown Format Byte=%d", fb));
    }
  }

}  // namespace

std::string DiskCopyHeader::DebugString() const {
  std::string result;
  std::string_view name(name_bytes_,
			std::min(name_length_,
				 DiskCopyHeader::kMaxNameLength));
  absl::StrAppendFormat(&result, "name[%d]: %v\n", name_length_, name);
  absl::StrAppendFormat(&result, "0x%x data bytes (%d k)\n", data_size_,
			data_size_ >> 10);
  absl::StrAppendFormat(&result, "0x%x tag bytes (%d k)\n", tag_size_,
			tag_size_ >> 10);
  absl::StrAppendFormat(&result, "Data Checksum: %x Tag Checksum: %x\n",
			header_data_checksum_, header_tag_checksum_);
  const auto disk_format = DiskFormatByte(disk_format_);
  absl::StrAppendFormat(&result, "Disk Format: %d (%s)\n", disk_format_,
			disk_format.ok() ? *disk_format : "<unknown>");
  const auto format_byte = FormatByte(format_byte_);
  absl::StrAppendFormat(&result, "Format Byte: %d (%s)\n", format_byte_,
			format_byte.ok() ? *format_byte : "<unknown>");
  absl::StrAppendFormat(&result, "Private word: 0x%x\n", private_);
  return result;
}

uint32_t DiskCopyHeader::TotalFileSize() const {
  return data_size_ + tag_size_ + kHeaderLength;
}

absl::StatusOr<uint32_t> DiskCopyHeader::Validate() const {
  if (name_length_ > kMaxNameLength) {
    return absl::InvalidArgumentError(
      absl::StrFormat("Invalid name length = 5d", name_length_));
  }
  auto disk_format = DiskFormatByte(disk_format_);
  if (!disk_format.ok()) return disk_format.status();
  auto format = FormatByte(format_byte_);
  if (!format.ok()) return format.status();
  if (private_ != kPrivate)
    return absl::InvalidArgumentError(absl::StrFormat(
     "Invalid magic number %s != 0x100", private_));
  return TotalFileSize();
}

uint32_t DiskCopyChecksum::UpdateSum(uint16_t new_word) {
  // "For each data REVERSE WORD:
  //      Add the data REVERSE WORD to the checksum
  //      Rotate the 32-bit checksum right one bit (wrapping bit 0 to bit 31)
  uint32_t s = sum_ + new_word;
  if (s & 0x1) {
    sum_ = 0x80000000 | ((s >> 1) & 0x7fffffff);
  } else {
    sum_ = 0x7fffffff & (s >> 1);
  }
  return sum_;
}

absl::Status DiskCopyHeader::VerifyDataChecksum(std::ifstream& s) {
  if (data_size_ & 0x1) {
    return absl::FailedPreconditionError(
      absl::StrFormat("Data size %d bytes is not a multiple of 2 bytes",
		      data_size_));
  }
  s.seekg(kHeaderLength);
  if (s.fail()) {
    return absl::OutOfRangeError(
      absl::StrFormat("Could not seek to %d bytes", kHeaderLength));
  }

  constexpr size_t kChunkSize = 1024;
  char buf[kChunkSize];
  size_t remaining_bytes_to_read = data_size_;
  DiskCopyChecksum sum(0);
  size_t data_bytes_read = 0;
  
  while (remaining_bytes_to_read) {
    size_t chunk_size = std::min(remaining_bytes_to_read, kChunkSize);
    if (!s.read(buf, chunk_size)) {
      return absl::OutOfRangeError(absl::StrFormat(
	"Failed to read %d bytes after %d bytes read, %d bytes remaining",
	chunk_size, data_bytes_read, remaining_bytes_to_read));
    }
    for (size_t c = 0; c < chunk_size; c+=2) {
      sum.UpdateSum(BigEndian2(buf + c));
    }
    data_bytes_read += chunk_size;
    remaining_bytes_to_read -= chunk_size;
  }
  uint32_t computed_sum = sum.Sum();
  if (computed_sum != header_data_checksum_) {
    return absl::NotFoundError(absl::StrFormat(
      "Computed data checksum %x does not match header sum %x",
      computed_sum, header_data_checksum_));
  }
  return absl::OkStatus();
}

      

#include "disk_copy.h"

#include <fstream>

#include "absl/strings/str_cat.h"
#include "endian.h"

DiskCopyHeader::DiskCopyHeader(const char header_bytes[kHeaderLength]) {
  name_length_ = header_bytes[0];
  memcpy(name_bytes_, header_bytes + 1, kMaxNameLength);
  data_size_ = BigEndian4(header_bytes + 64);
  tag_size_ = BigEndian4(header_bytes + 68);
  header_data_checksum_ = BigEndian4(header_bytes + 72);
  header_tag_checksum_ = BigEndian4(header_bytes + 76);
  disk_format_ = header_bytes[80];
  format_byte_ = header_bytes[81];
  private_ = BigEndian2(header_bytes + 82);
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

absl::Status DiskCopyHeader::WriteToDisk(std::ofstream& s) {
  char header_bytes[kHeaderLength];
  header_bytes[0] = name_length_;
  memcpy(header_bytes + 1, name_bytes_, kMaxNameLength);
  WriteBigEndian4(data_size_, header_bytes + 64);
  WriteBigEndian4(tag_size_, header_bytes + 68);
  WriteBigEndian4(header_data_checksum_, header_bytes + 72);
  WriteBigEndian4(header_tag_checksum_, header_bytes + 76);
  header_bytes[80] = disk_format_;
  header_bytes[81] = format_byte_;
  WriteBigEndian2(private_, header_bytes + 82);
  if (!s.write(header_bytes, kHeaderLength)) {
    return absl::ResourceExhaustedError("Could not write DiskCopyHeader");
  }
  return absl::OkStatus();
}

absl::StatusOr<DiskCopyHeader> DiskCopyHeader::CreateForHFS(
    const absl::string_view name, const uint32_t data_block_count,
    const uint32_t data_checksum, const uint32_t tag_byte_count,
    const uint32_t tag_checksum) {
  const size_t name_length = name.length();
  if (name_length > kMaxNameLength) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "name '%s' length %d is longer than the DC42 maximum %d", name,
        name_length, kMaxNameLength));
  }
  uint8_t disk_format_byte;
  uint8_t format_byte;
  if (800 == data_block_count) {
    disk_format_byte = 0;
    format_byte = 0x12;  // Follow Apple File Type Note
  } else if (1600 == data_block_count) {
    disk_format_byte = 1;
    format_byte = 0x22;
  } else if (1440 == data_block_count) {
    disk_format_byte = 2;
    format_byte = 0x22;
  } else if (2880 == data_block_count) {
    disk_format_byte = 3;
    format_byte = 0x22;
  } else {
    return absl::InvalidArgumentError(
        absl::StrFormat("HFS data block count %d is not recognized as valid",
                        data_block_count));
  }
  return DiskCopyHeader(name, data_block_count * 512, tag_byte_count,
                        data_checksum, tag_checksum, disk_format_byte,
                        format_byte);
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
          absl::StrFormat("Unknown Disk Format Byte=%d", dfb));
  }
}

absl::StatusOr<std::string> FormatByte(const uint8_t fb) {
  switch (fb) {
    case 0x2:
      return "400k (alternate)";  // 68k MLA claim.
    case 0x12:
      return "400k";  // Apple FTN doc claim
    case 0x22:
      return ">400k";
    case 0x24:
      return "800k Apple II";
    default:
      return absl::InvalidArgumentError(
          absl::StrFormat("Unknown Format Byte=%d", fb));
  }
}

absl::Status CheckEven(uint32_t byte_count) {
  if (byte_count % 1) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Data size %d is not an even number of bytes.", byte_count));
  }
  return absl::OkStatus();
}

}  // namespace

std::string DiskCopyHeader::DebugString() const {
  std::string result;
  std::string_view name(name_bytes_,
                        std::min(name_length_, DiskCopyHeader::kMaxNameLength));
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
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid magic number %s != 0x100", private_));
  auto valid_data_size = CheckEven(data_size_);
  if (!valid_data_size.ok()) {
    return valid_data_size;
  }
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

absl::Status DiskCopyChecksum::UpdateSumFromBlock(const char* buffer,
                                                  uint32_t byte_count) {
  auto byte_count_status = CheckEven(byte_count);
  if (!byte_count_status.ok()) {
    return byte_count_status;
  }
  for (size_t c = 0; c < byte_count; c += 2) {
    UpdateSum(BigEndian2(buffer + c));
  }
  return absl::OkStatus();
}

absl::Status DiskCopyChecksum::UpdateSumFromFile(std::ifstream& s,
                                                 uint32_t byte_count) {
  auto byte_count_status = CheckEven(byte_count);
  if (!byte_count_status.ok()) {
    return byte_count_status;
  }

  constexpr size_t kChunkSize = 1024;
  char buf[kChunkSize];
  size_t remaining_bytes_to_read = byte_count;
  size_t data_bytes_read = 0;

  while (remaining_bytes_to_read) {
    size_t chunk_size = std::min(remaining_bytes_to_read, kChunkSize);
    if (!s.read(buf, chunk_size)) {
      return absl::OutOfRangeError(absl::StrFormat(
          "Failed to read %d bytes after %d bytes read, %d bytes remaining",
          chunk_size, data_bytes_read, remaining_bytes_to_read));
    }
    auto sum_status = UpdateSumFromBlock(buf, chunk_size);
    if (!sum_status.ok()) {
      return sum_status;
    }
    data_bytes_read += chunk_size;
    remaining_bytes_to_read -= chunk_size;
  }
  return absl::OkStatus();
}

absl::Status DiskCopyHeader::VerifyDataChecksum(std::ifstream& s) {
  s.seekg(kHeaderLength);
  if (s.fail()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Could not seek to %d bytes", kHeaderLength));
  }

  DiskCopyChecksum sum(0);
  auto status = sum.UpdateSumFromFile(s, data_size_);
  if (!status.ok()) {
    return status;
  }
  uint32_t computed_sum = sum.Sum();
  if (computed_sum != header_data_checksum_) {
    return absl::NotFoundError(absl::StrFormat(
        "Computed data checksum %x does not match header sum %x", computed_sum,
        header_data_checksum_));
  }
  return absl::OkStatus();
}

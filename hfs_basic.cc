#include "hfs_basic.h"

#include <fstream>

#include "absl/strings/str_cat.h"
#include "endian.h"

HFSMasterDirectoryBlock::HFSMasterDirectoryBlock(
    const char mdb_bytes[kMDBBytes]) {
  signature_ = BigEndian2(mdb_bytes);
  volume_creation_date_ = BigEndian4(mdb_bytes + 2);
  last_modification_date_ = BigEndian4(mdb_bytes + 6);
  volume_attributes_ = BigEndian2(mdb_bytes + 10);
  num_files_root_directory_ = BigEndian2(mdb_bytes + 12);
  volume_bitmap_block_ = BigEndian2(mdb_bytes + 14);
  next_allocation_search_ = BigEndian2(mdb_bytes + 16);
  num_allocation_blocks_ = BigEndian2(mdb_bytes + 18);
  allocation_block_size_ = BigEndian4(mdb_bytes + 20);
  default_clump_size_ = BigEndian4(mdb_bytes + 24);
  first_allocation_block_ = BigEndian2(mdb_bytes + 28);
  next_unused_catalog_node_id_ = BigEndian4(mdb_bytes + 30);
  num_free_allocation_blocks_ = BigEndian2(mdb_bytes + 34);
  volume_name_length_ = mdb_bytes[36];
  memcpy(volume_name_bytes_, mdb_bytes + 37, kMaxVolumeNameLength);
}

// static
absl::StatusOr<HFSMasterDirectoryBlock> HFSMasterDirectoryBlock::ReadFromDisk(
    std::ifstream& s) {
  s.seekg(kMDBOffset);
  if (s.fail()) {
    return absl::OutOfRangeError(
        "Could not seek to HFS Master Directory Block");
  }
  char mdb_bytes[kMDBBytes];
  if (!s.read(mdb_bytes, kMDBBytes)) {
    return absl::OutOfRangeError(
        absl::StrCat("Could not read ", kMDBBytes, " bytes"));
  }
  return HFSMasterDirectoryBlock(mdb_bytes);
}

absl::StatusOr<std::string> HFSMasterDirectoryBlock::VolumeName() const {
  if (volume_name_length_ > kMaxVolumeNameLength) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Declared volume name length %d > maximum %d",
                        volume_name_length_, kMaxVolumeNameLength));
  }
  std::string_view name(volume_name_bytes_, volume_name_length_);
  return std::string(name);
}

std::string HFSMasterDirectoryBlock::DebugString() const {
  std::string result;
  std::string_view name(volume_name_bytes_,
                        std::min(volume_name_length_, kMaxVolumeNameLength));
  absl::StrAppendFormat(&result, "name[%d]: %v\n", volume_name_length_, name);
  absl::StrAppendFormat(&result, "%d allocation blocks each %d bytes\n",
                        num_allocation_blocks_, allocation_block_size_);
  absl::StrAppendFormat(&result, "%d first allocation block\n",
                        first_allocation_block_);
  absl::StrAppendFormat(&result, "%d free allocation blocks\n",
                        num_free_allocation_blocks_);

  return result;
}

absl::StatusOr<uint64_t> HFSMasterDirectoryBlock::Valid() const {
  if (signature_ != kHFSSignature) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Signature %x did not match magic number %x",
                        signature_, kHFSSignature));
  }
  uint16_t mod = allocation_block_size_ % kHFSBlockSize;
  if (mod != 0) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Declared allocation size %d not a multiple of block size %d",
        allocation_block_size_, kHFSBlockSize));
  }
  // The first allocation block # is a count of blocks at the start of the
  // volume (2 boot blocks, the MDB block, at least one volume bitmap block).
  // Two blocks at the end of the disk are also unavailable:
  // One is a backup copy of the MDB, the very last is reserved for Apple.
  const uint32_t non_allocated_blocks = (first_allocation_block_ + 2);
  const uint64_t allocation_hfs_blocks =
      (allocation_block_size_ / kHFSBlockSize) * num_allocation_blocks_;
  return non_allocated_blocks + allocation_hfs_blocks;
}

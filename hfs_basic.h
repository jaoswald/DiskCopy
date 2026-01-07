// Very basic support for HFS floppy volumes.
// disk_copy needs to extract the volume name from an HFS volume, but not
// much else.

#include <cstdint>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"

class HFSMasterDirectoryBlock {
 public:
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const HFSMasterDirectoryBlock& mdb) {
    absl::Format(&sink, "%s", mdb.DebugString());
  }

  // Human readable description.
  std::string DebugString() const;

  // Read MDB from a (binary-format) file stream; assumes the stream contains
  // a raw image, seeks to the first MDB offset at byte offset 1024 (logical
  // block 2)
  static absl::StatusOr<HFSMasterDirectoryBlock>
    ReadFromDisk(std::ifstream& s);

  // Returns an error if the volume name cannot be extracted (has invalid
  // length). Should also check Valid() before relying on this.
  absl::StatusOr<std::string> VolumeName() const;

  // Checks for basic validity, returns declared size of volume in 512-byte
  // blocks if valid.
  absl::StatusOr<uint64_t> Valid() const;

 private:
  static constexpr uint16_t kMDBBytes = 512;
  static constexpr uint16_t kHFSBlockSize = 512;  // bytes
  static constexpr uint16_t kHFSSignature = 0x4244;
  static constexpr size_t kMaxVolumeNameLength = 27;
  // Start of MDB from start of image.
  static constexpr size_t kMDBOffset = 1024;

  explicit HFSMasterDirectoryBlock(const char mdb_bytes[kMDBBytes]);

  uint16_t signature_;  // should be kHFSSignature.
  uint32_t volume_creation_date_;
  uint32_t last_modification_date_;
  uint16_t volume_attributes_;
  uint16_t num_files_root_directory_;
  uint16_t volume_bitmap_block_;
  uint16_t next_allocation_search_;
  uint16_t num_allocation_blocks_;
  // size of allocation block (in bytes)
  uint32_t allocation_block_size_;
  uint32_t default_clump_size_;
  uint16_t first_allocation_block_;
  uint32_t next_unused_catalog_node_id_;
  uint16_t num_free_allocation_blocks_;
  size_t volume_name_length_;
  char volume_name_bytes_[kMaxVolumeNameLength];
  // Don't care about the rest.
};

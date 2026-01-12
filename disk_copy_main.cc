/* Tool to manipulate Apple classic 68k Macintosh 'Disk Copy' images.

   See README.md for more information.
   See LICENSE for details on the (MIT-style) license for this software.

   Copyright (c) 2026 Joseph A. Oswald, III <josephoswald@gmail.com>
   https://github.com/jaoswald/DiskCopy

*/

#include <fstream>
#include <iostream>
#include <istream>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "disk_copy.h"
#include "hfs_basic.h"

ABSL_FLAG(bool, ignore_data_checksum, false,
          "If true, extract data from the --disk_copy file without regard for "
          "the data checksum validity.");

ABSL_FLAG(std::string, disk_copy, "",
          "Path name of Disk Copy 4.2 (`DC42`) image file to read or produce");
ABSL_FLAG(std::string, output_image, "",
          "Path name of raw HFS disk image to produce by extracting from "
          "--disk_copy.");
ABSL_FLAG(std::string, input_image, "",
          "Path name of raw HFS disk image to encode into --disk_copy.");

namespace {

using std::cerr;
using std::string_view;

enum class Command { CREATE, EXTRACT, VERIFY };

absl::StatusOr<Command> ParseCommand(const string_view c) {
  if (c == "create") {
    return Command::CREATE;
  } else if (c == "extract") {
    return Command::EXTRACT;
  } else if (c == "verify") {
    return Command::VERIFY;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Unrecognized command `", c, "`"));
}

absl::Status CreateCommand(const string_view input_image,
                           const string_view disk_copy) {
  if (input_image.empty() || disk_copy.empty()) {
    return absl::InvalidArgumentError(
        "Create requires --input_image and --disk_copy.");
  }
  std::ifstream input(input_image.data(), std::ios::binary);
  if (!input.good()) {
    return absl::NotFoundError(
        absl::StrCat("Could not open input_image '", input_image, "'"));
  }
  auto hfsmdb = HFSMasterDirectoryBlock::ReadFromDisk(input);
  if (!hfsmdb.ok()) {
    return hfsmdb.status();
  }
  absl::PrintF("Read HFS MDB: %v\n", *hfsmdb);
  auto hfs_block_count = hfsmdb->Valid();
  if (!hfs_block_count.ok()) {
    return hfs_block_count.status();
  }
  auto hfs_name = hfsmdb->VolumeName();
  if (!hfs_name.ok()) {
    return hfs_name.status();
  }
  absl::PrintF("HFS volume '%s' declared to be %d disk blocks.\n", *hfs_name,
               *hfs_block_count);

  // Compute data checksum from HFS file.
  input.seekg(0);
  if (input.fail()) {
    return absl::FailedPreconditionError("Could not seek HFS image stream.");
  }
  DiskCopyChecksum sum(0);
  // Compute the data checksum for the whole HFS data so we have it for the
  // header; seeking back to update the checksum might save some I/O cost.
  auto checksum_status = sum.UpdateSumFromFile(input, *hfs_block_count * 512);
  if (!checksum_status.ok()) {
    return checksum_status;
  }
  auto dch =
      DiskCopyHeader::CreateForHFS(*hfs_name, *hfs_block_count, sum.Sum());

  std::ofstream output(disk_copy.data(), std::ios::binary);
  if (!output.good()) {
    return absl::ResourceExhaustedError(
        absl::StrCat("Could not write disk_copy '", disk_copy, "'"));
  }
  auto header_status = dch->WriteToDisk(output);
  if (!header_status.ok()) {
    return header_status;
  }

  // There has to be some way to use std::copy or whatever on
  // streambuf_iterators, and a known number of bytes?
  // std::istreambuf_iterator<char> input_iterator{input};
  // std::ostreambuf_iterator<char> output_iterator{output};
  // std::copy(input_iterator, input_iterator + (*hfs_block_count * 512),
  // 	    output_iterator);
  // how to error check?

  input.seekg(0);
  static constexpr size_t kBlockSize = 512;
  char buf[kBlockSize];
  for (uint32_t b = 0; b < *hfs_block_count; ++b) {
    if (!input.read(buf, kBlockSize)) {
      return absl::OutOfRangeError(
          absl::StrFormat("Could not read block %d of HFS input", b));
    }
    if (!output.write(buf, kBlockSize)) {
      return absl::ResourceExhaustedError(
          absl::StrFormat("Could not write block %d of Disk Copy output", b));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<uint32_t> ExtractCommand(const string_view disk_copy,
                                        const string_view output_image,
                                        const bool ignore_data_checksum) {
  if (disk_copy.empty() || output_image.empty()) {
    return absl::InvalidArgumentError(
        "Extract requires --disk_copy and --output_image");
  }
  std::ifstream input(disk_copy.data(), std::ios::binary);
  if (!input.good()) {
    return absl::NotFoundError(
        absl::StrCat("Could not open disk_copy '", disk_copy, "'"));
  }
  auto header = DiskCopyHeader::ReadFromDisk(input);
  if (!header.ok()) {
    return header.status();
  }
  auto header_valid = header->Validate();
  if (!header_valid.ok()) {
    return header_valid;
  }
  std::ofstream output_hfs(output_image.data(), std::ios::binary);
  if (!output_hfs.good()) {
    return absl::ResourceExhaustedError(
        absl::StrCat("Could not open output_image '", output_image, "'"));
  }
  // DiskCopy should be at the end of the header, ready to read data.
  const uint32_t total_bytes_to_read = header->DataSize();
  uint32_t bytes_remaining = total_bytes_to_read;

  // Prepare to compute the Disk Copy data checksum as we read.
  DiskCopyChecksum sum(0);
  static constexpr uint32_t kBlockSize = 512;
  char buf[kBlockSize];
  while (bytes_remaining > 0) {
    size_t read_size = std::min(bytes_remaining, kBlockSize);
    if (!input.read(buf, read_size)) {
      return absl::OutOfRangeError(
          absl::StrFormat("Could not read %d bytes of Disk Copy input after %d",
                          read_size, total_bytes_to_read - bytes_remaining));
    }
    bytes_remaining -= read_size;
    // header_valid call above should mean the only possible error has
    // already been checked (sum is computed over 16-bit words, so an odd
    // number of bytes is an error; kBlockSize is even, so read_size will
    // be even.
    absl::Status sum_status = sum.UpdateSumFromBlock(buf, read_size);
    if (!sum_status.ok()) {
      return sum_status;
    }
    if (!output_hfs.write(buf, read_size)) {
      return absl::ResourceExhaustedError(
          absl::StrFormat("Could not write %d bytes of HFS image output at %d",
                          read_size, total_bytes_to_read - bytes_remaining));
    }
  }
  const uint32_t expected_data_checksum = header->ExpectedDataChecksum();
  const uint32_t computed_data_checksum = sum.Sum();
  if (computed_data_checksum != expected_data_checksum) {
    std::string message =
        absl::StrFormat("Disk Copy data checksum %x does not match header %x",
                        computed_data_checksum, expected_data_checksum);
    cerr << message << std::endl;
    if (ignore_data_checksum) {
      cerr << "Ignoring mismatch because of --ignore_data_checksum"
           << std::endl;
    } else {
      return absl::FailedPreconditionError(message);
    }
  }
  return total_bytes_to_read;
}

absl::Status VerifyCommand(const string_view disk_copy) {
  if (disk_copy.empty()) {
    return absl::InvalidArgumentError("Verify requires --disk_copy");
  }
  std::ifstream f(disk_copy.data(), std::ios::binary);
  if (!f.good()) {
    return absl::NotFoundError(
        absl::StrCat("Could not open disk_copy '", disk_copy, "'"));
  }
  auto header = DiskCopyHeader::ReadFromDisk(f);
  if (!header.ok()) {
    return header.status();
  }
  absl::PrintF("Read header: %v\n", *header);
  return header->VerifyDataChecksum(f);
  // TODO: VerifyTagChecksum if tag bytes present.
}

}  // namespace

int main(int argc, char* argv[]) {
  absl::SetProgramUsageMessage(absl::StrCat(
      "This program converts between uncompressed Apple Disk Copy 4.2 "
      "`DC42` disk images.\n\n",
      argv[0],
      " <command>\n\n"
      "Supported commands:\n\n"
      "  `create`  : use data in --input_image argument to create --disk_copy\n"
      "  `extract` : extract data from --disk_copy argument into "
      "--output_image\n"
      "  `verify`  : validate basic structure and checksums for "
      "--disk_copy\n"));

  std::vector<char*> positional_args = absl::ParseCommandLine(argc, argv);
  const int arg_count = positional_args.size();  // includes program name

  if (arg_count < 2) {
    cerr << "Requires a command.";
    cerr << absl::ProgramUsageMessage();
    return 1;
  }
  if (arg_count > 2) {
    cerr << "Too many arguments " << arg_count - 1 << std::endl;
    cerr << absl::ProgramUsageMessage();
    return 1;
  }
  auto cmd = ParseCommand(positional_args[1]);
  if (!cmd.ok()) {
    cerr << cmd.status() << std::endl;
    cerr << absl::ProgramUsageMessage();
    return 1;
  }
  const bool ignore_data_checksum = absl::GetFlag(FLAGS_ignore_data_checksum);

  absl::Status status;
  switch (cmd.value()) {
    case Command::CREATE:
      if (ignore_data_checksum) {
        status = absl::InvalidArgumentError(
            "'create' cannot use --ignore-data-checksum");
        break;
      }
      status = CreateCommand(absl::GetFlag(FLAGS_input_image),
                             absl::GetFlag(FLAGS_disk_copy));
      break;
    case Command::EXTRACT: {
      auto bytes_read = ExtractCommand(absl::GetFlag(FLAGS_disk_copy),
                                       absl::GetFlag(FLAGS_output_image),
                                       ignore_data_checksum);
      if (bytes_read.ok()) {
        cerr << "Read " << *bytes_read << " bytes (" << (*bytes_read / 512)
             << ") HFS blocks." << std::endl;
      } else {
        status = bytes_read.status();
      }
    } break;
    case Command::VERIFY:
      if (ignore_data_checksum) {
        status = absl::InvalidArgumentError(
            "'verify' cannot use --ignore-data-checksum");
        break;
      }
      status = VerifyCommand(absl::GetFlag(FLAGS_disk_copy));
      break;
    default:
      status = absl::InvalidArgumentError("unknown command");
  }
  if (!status.ok()) {
    cerr << status << std::endl;
    return 2;
  }
  return 0;
}

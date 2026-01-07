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

ABSL_FLAG(bool, ignore_data_checksum, false,
	  "If true, extract data from the --disk_copy file without regard for "
	  "the data checksum validity.");

ABSL_FLAG(std::string, disk_copy, "", "Path name of Disk Copy 4.2 (`DC42`) "
	  "image file to read or produce");
ABSL_FLAG(std::string, output_image, "", "Path name of raw HFS disk image to "
	  "produce by extracting from --disk_copy.");
ABSL_FLAG(std::string, input_image, "", "Path name of raw HFS disk image to "
	  "encode into --disk_copy.");

namespace {

using std::cerr;
using std::string_view;
 
enum class Command {
  CREATE,
  EXTRACT,
  VERIFY
};

absl::StatusOr<Command> ParseCommand(const string_view c) {
  if (c == "create") {
    return Command::CREATE;
  } else if (c == "extract") {
    return Command::EXTRACT;
  } else if (c == "verify") {
    return Command::VERIFY;
  }
  return absl::InvalidArgumentError(absl::StrCat("Unrecognized command `",
						 c, "`"));
}

absl::Status CreateCommand(const string_view input_image,
			   const string_view disk_copy) {
  if (input_image.empty() || disk_copy.empty()) {
    return absl::InvalidArgumentError(
       "Create requires --input_image and --disk_copy.");
  }
  return absl::OkStatus();
}

absl::Status VerifyCommand(const string_view disk_copy) {
  if (disk_copy.empty()) {
    return absl::InvalidArgumentError("Verify requires --disk_copy");
  }
  std::ifstream f(disk_copy.data(), std::ios::binary);
  if (!f.good()) {
    return absl::NotFoundError(absl::StrCat("Could not open disk_copy '",
					    disk_copy, "'"));
  }
  auto header = DiskCopyHeader::ReadFromDisk(f);
  if (!header.ok()) {
    return header.status();
  }
  absl::PrintF("Read header: %v\n", *header);
  return header->VerifyDataChecksum(f);
}

}  // namespace

int main(int argc, char* argv[]) {
  absl::SetProgramUsageMessage(absl::StrCat(
     "This program converts between uncompressed Apple Disk Copy 4.2 "
     "`DC42` disk images.\n\n", argv[0], " <command>\n\n"
     "Supported commands:\n\n"
     "  `create`  : use data in --input_image argument to create --disk_copy\n"
     "  `extract` : extract data from --disk_copy argument into --output_image\n"
     "  `verify`  : validate basic structure and checksums for --disk_copy\n"));

  std::vector<char*> positional_args = absl::ParseCommandLine(argc, argv);
  const int arg_count = positional_args.size(); // includes program name

  if (arg_count < 2) {
    cerr << "Requires a command.";
    cerr << absl::ProgramUsageMessage();
    return 1;
  }
  if (arg_count > 2) {
    cerr << "Too many arguments " << arg_count-1 << std::endl;
    cerr << absl::ProgramUsageMessage();
    return 1;
  }
  auto cmd = ParseCommand(positional_args[1]);
  if (!cmd.ok()) {
    cerr << cmd.status() << std::endl;
    cerr << absl::ProgramUsageMessage();
    return 1;
  }

  absl::Status status;
  switch (cmd.value()) {
  case Command::CREATE:
    status = CreateCommand(absl::GetFlag(FLAGS_input_image),
			   absl::GetFlag(FLAGS_disk_copy));
    break; 
  case Command::EXTRACT:
    status = absl::UnimplementedError("extract");
    break;
  case Command::VERIFY:
    if (absl::GetFlag(FLAGS_ignore_data_checksum)) {
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

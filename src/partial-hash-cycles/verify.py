#!/usr/bin/env python3

import sys
import re
import struct
import hashlib

# Seed used by your C++ code
SEED = 12345

# Regex to find run headers, like "--- Run 1 ---"
RUN_HEADER_REGEX = re.compile(r'^--- Run\s+(\d+)\s*---$')

# Regex to capture lines of the form:
#   Digest[x] (anything): <hex bytes possibly with < > around them>
# We'll parse out the index (x) and the rest of the line containing the hex bytes.
DIGEST_LINE_REGEX = re.compile(r'^Digest\[(\d+)\].*:\s*(.*)$')

def parse_hex_string(hex_str):
  """
  Given a string like 'db 49 <b6> 6d',
  remove angle brackets and convert to raw bytes.
  """
  cleaned = hex_str.replace("<", "").replace(">", "")
  parts = cleaned.strip().split()
  return bytes(int(p, 16) for p in parts)

def process_run(run_lines, run_number):
  """
  Parse the lines for one run, gather all digests, and verify consecutive links.
  """
  # We'll store each digest in a dict of:
  #   index -> (digest_bytes, original_line)
  digests_by_index = {}

  for line in run_lines:
    line = line.strip()
    m = DIGEST_LINE_REGEX.match(line)
    if m:
      idx_str = m.group(1)
      hex_data_str = m.group(2)
      digest_idx = int(idx_str)

      digest_bytes = parse_hex_string(hex_data_str)
      digests_by_index[digest_idx] = (digest_bytes, line)

  # Sort by index so we can verify consecutive pairs
  sorted_indices = sorted(digests_by_index.keys())

  print(f"\n== Checking Run {run_number} ({len(sorted_indices)} total Digest lines) ==")

  for i in range(len(sorted_indices) - 1):
    current_idx = sorted_indices[i]
    next_idx = sorted_indices[i + 1]

    if next_idx == current_idx + 1:
      current_digest, current_line = digests_by_index[current_idx]
      next_digest, next_line = digests_by_index[next_idx]

      # Show the lines we are verifying
      print(f"\nComparing consecutive digests:\n  {current_line}\n  {next_line}")

      # Our custom hashing logic: SHA256( seed[4 bytes little-endian] + current_digest[32 bytes] )
      data_to_hash = struct.pack("<I", SEED) + current_digest
      computed = hashlib.sha256(data_to_hash).digest()

      if computed == next_digest:
        print(f"  Digest[{current_idx}] -> Digest[{next_idx}] OK")
      else:
        print(f"  Digest[{current_idx}] -> Digest[{next_idx}] INVALID")

def main():
  runs = []  # list of (run_number, [lines for that run])
  current_run_lines = []
  current_run_number = None

  for line in sys.stdin:
    line_stripped = line.strip()
    # Check if it's a run header
    header_match = RUN_HEADER_REGEX.match(line_stripped)
    if header_match:
      # We have a new run header
      new_run_number = int(header_match.group(1))

      # If we had a previous run in progress, store it
      if current_run_number is not None:
        runs.append((current_run_number, current_run_lines))

      # Start a new run
      current_run_number = new_run_number
      current_run_lines = [line.rstrip("\n")]  # include this line in the run
    else:
      # Just another line of the current run
      current_run_lines.append(line.rstrip("\n"))

  # End of file. Store the last run if any
  if current_run_number is not None:
    runs.append((current_run_number, current_run_lines))

  # Now process each run individually
  for run_number, run_lines in runs:
    process_run(run_lines, run_number)

  print("\nDone verifying all runs.")

if __name__ == "__main__":
  main()


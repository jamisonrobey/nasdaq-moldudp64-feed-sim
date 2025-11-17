#!/usr/bin/env python3

# Takes a larger TotalView-ITCH file in emi.nasdaq.com format and
# skips messages before 9:30am and writes the next 1000 messages
# to a new binary file.

import argparse
from pathlib import Path
import sys

len_prefix_size = 2    # bytes
timestamp_size = 6
# 1 byte msg type + 2 bytes stock locate + 2 bytes tracking number = 5
timestamp_offset_in_msg = 1 + 2 + 2

ns_since_midnight_9_30am = 34_200_000_000_000
target_message_count = 1000


def read_message(f):
    prefix_bytes = f.read(len_prefix_size)
    if len(prefix_bytes) < len_prefix_size:
        return None, None

    length = int.from_bytes(prefix_bytes, "big")
    msg_bytes = f.read(length)
    if len(msg_bytes) < length:
        # truncated file
        return None, None

    return prefix_bytes, msg_bytes


def extract_timestamp_ns(msg_bytes):
    start = timestamp_offset_in_msg
    end = start + timestamp_size
    ts_bytes = msg_bytes[start:end]
    if len(ts_bytes) < timestamp_size:
        # malformed message
        return None
    return int.from_bytes(ts_bytes, "big")


def iter_messages(fin):
    while True:
        prefix_bytes, msg_bytes = read_message(fin)
        if prefix_bytes is None:
            break
        yield prefix_bytes, msg_bytes


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Create a 1000-msg ITCH snippet starting at 9:30am."
    )
    parser.add_argument(
        "input_file",
        type=Path,
        help="Path to the ITCH file",
    )
    parser.add_argument(
        "output_file",
        type=Path,
        help="Path to write the snippet binary",
    )

    args = parser.parse_args()

    if not args.input_file.is_file():
        print(f"Error: file not found: {args.input_file}", file=sys.stderr)
        sys.exit(1)

    messages_written = 0
    started = False

    with args.input_file.open("rb") as fin, args.output_file.open("wb") as fout:
        for prefix_bytes, msg_bytes in iter_messages(fin):
            ts_ns = extract_timestamp_ns(msg_bytes)
            if ts_ns is None:
                continue

            if not started:
                if ts_ns < ns_since_midnight_9_30am:
                    continue
                started = True

            fout.write(prefix_bytes)
            fout.write(msg_bytes)
            messages_written += 1

            if messages_written >= target_message_count:
                break

    print(f"Wrote {messages_written} messages to {args.output_file}")

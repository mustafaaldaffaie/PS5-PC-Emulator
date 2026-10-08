from __future__ import annotations

import pathlib
import struct
import subprocess
import sys
import tempfile


def executable_fixture() -> bytes:
    data = bytearray(0x104)
    data[:7] = b"\x7fELF\x02\x01\x01"
    struct.pack_into("<HHIQQQIHHHHHH", data, 16,
                     3, 62, 1, 0x1000, 64, 0, 0, 64, 56, 1, 0, 0, 0)
    struct.pack_into("<IIQQQQQQ", data, 64,
                     1, 5, 0x100, 0x1000, 0, 4, 16, 0x100)
    data[0x100:] = b"\x90\x90\x90\xc3"
    return bytes(data)


def main() -> int:
    executable = str(pathlib.Path(sys.argv[1]).resolve())

    def run(arguments: list[str], code: int, expected: str) -> None:
        result = subprocess.run([executable, *arguments], capture_output=True,
                                text=True, timeout=15)
        if result.returncode != code or expected not in result.stdout + result.stderr:
            raise RuntimeError(
                f"CLI failed for {arguments}: code={result.returncode}\n"
                f"{result.stdout}{result.stderr}")

    with tempfile.TemporaryDirectory(prefix="ps5emu-cli-") as directory:
        sample = pathlib.Path(directory) / "sample.elf"
        sample.write_bytes(executable_fixture())
        run(["inspect", str(sample)], 0, "Entry point: 0x1000")
        run(["prepare", str(sample)], 0, "Prepared entry point: 0x1000")
        run(["prepare", str(sample), "0x500000"], 0,
            "Prepared entry point: 0x501000")
        run(["prepare", str(sample), "5242880"], 0,
            "Prepared entry point: 0x501000")
        run(["prepare", str(sample), "0X500000"], 0,
            "Guest execution is not implemented.")
        for bias in ["0x", "-1", "garbage", "18446744073709551616"]:
            run(["prepare", str(sample), bias], 2, "Invalid load bias")
        run(["prepare", str(sample), "0xffffffffffffffff"], 2,
            "ELF load address overflows")
        run(["prepare", str(sample.with_name("missing.elf"))], 2,
            "Failed to open executable file")
        run(["unknown"], 1, "Invalid command line")
        sample.write_bytes(b"not an executable")
        run(["prepare", str(sample)], 2, "ELF file is too small")
    print("CLI integration tests passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

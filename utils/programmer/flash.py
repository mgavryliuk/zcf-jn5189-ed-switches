#!/usr/bin/env python3
import argparse
import logging
from pathlib import Path

from isp.interface import ISPInterface


def main(cli_args):
    firmware_path = Path(cli_args.firmware_path).resolve()
    if not firmware_path.exists():
        raise argparse.ArgumentError("firmware_path", "File doesn't exist")

    with ISPInterface(port=cli_args.serial_port, baudrate=cli_args.baudrate) as client:
        client.initialize()
        client.get_info()
        client.erase()
        client.flash(firmware_path)
        if not cli_args.no_restart:
            client.restart()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Programmer to program JN5189")
    parser.add_argument("-s", "--serial-port", type=str, required=True, help="Serial port to use")
    parser.add_argument("-b", "--baudrate", type=int, default=115200, help="Baudrate")
    parser.add_argument("-n", "--no-restart", action="store_true", help="Do not restart device after flashing")
    parser.add_argument("firmware_path", type=str, help="Path to the firmware")

    logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(name)s: %(message)s")

    cli_args = parser.parse_args()
    main(cli_args)

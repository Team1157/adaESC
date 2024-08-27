# python test program thingy thats a tui and uses python-can with any of these https://python-can.readthedocs.io/en/stable/interfaces.html

import can
import time
import curses

def setup_can_bus():
    try:
        bus = can.interface.Bus(channel='can0', bustype='socketcan')
        print("CAN bus initialized successfully")
        return bus
    except can.CanError:
        print("Error initializing CAN bus. Make sure the CAN interface is set up correctly.")
        return None

def send_speed_command(bus, speed):
    # Pack speed (0-1000) into two bytes
    speed_bytes = speed.to_bytes(2, byteorder='big')
    message = can.Message(arbitration_id=0x123, data=speed_bytes, is_extended_id=False)
    try:
        bus.send(message)
        print(f"Sent speed command: {speed}")
    except can.CanError:
        print("Error sending CAN message")

def receive_status(bus):
    try:
        message = bus.recv(timeout=1.0)
        if message and message.arbitration_id == 0x456:  # Assuming 0x456 is the status message ID
            speed = int.from_bytes(message.data[0:2], byteorder='big')
            commutation_step = message.data[2]
            commutation_period = int.from_bytes(message.data[3:7], byteorder='big')
            return speed, commutation_step, commutation_period
    except can.CanError:
        print("Error receiving CAN message")
    return None, None, None

def main(stdscr):
    # Set up curses
    curses.curs_set(0)
    stdscr.nodelay(1)
    stdscr.timeout(100)

    # Initialize CAN bus
    bus = setup_can_bus()
    if not bus:
        return

    current_speed = 0
    while True:
        stdscr.clear()
        stdscr.addstr(0, 0, "Not rev hardware client (tm)")
        stdscr.addstr(2, 0, f"Current Speed: {current_speed}")
        stdscr.addstr(4, 0, "Commands:")
        stdscr.addstr(5, 0, "  + : Increase speed")
        stdscr.addstr(6, 0, "  - : Decrease speed")
        stdscr.addstr(7, 0, "  s : Stop motor")
        stdscr.addstr(8, 0, "  q : Quit")

        # Get status update
        speed, comm_step, comm_period = receive_status(bus)
        if speed is not None:
            stdscr.addstr(10, 0, f"Received Status:")
            stdscr.addstr(11, 0, f"  Speed: {speed}")
            stdscr.addstr(12, 0, f"  Commutation Step: {comm_step}")
            stdscr.addstr(13, 0, f"  Commutation Period: {comm_period} us")

        stdscr.refresh()

        # Handle user input
        key = stdscr.getch()
        if key != -1:
            if key == ord('+'):
                current_speed = min(1000, current_speed + 50)
                send_speed_command(bus, current_speed)
            elif key == ord('-'):
                current_speed = max(0, current_speed - 50)
                send_speed_command(bus, current_speed)
            elif key == ord('s'):
                current_speed = 0
                send_speed_command(bus, current_speed)
            elif key == ord('q'):
                break

        time.sleep(0.1)

if __name__ == "__main__":
    curses.wrapper(main)

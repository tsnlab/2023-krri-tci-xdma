from gpiozero import LED, Button, Buzzer
from time import sleep
import time
import sys

def read_gpio(gpio_num):
    button = Button(gpio_num)
    if button.is_pressed:
        print("Pressed")
        return 1
    else:
        print("Released")
        return 0

def write_gpio(gpio_num, setting, duration):
    led = Buzzer(gpio_num)
    if setting == 1:
        for _ in range(duration):
            led.on()
            sleep(1)
    else:
        led.off()

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python gpio_control.py <gpio_number> <rd/wr> [setting_value] <duration>")
        sys.exit(1)

    gpio_num = int(sys.argv[1])
    operation = sys.argv[2]

    if operation not in ('rd', 'wr'):
        print("Operation must be 'rd' or 'wr'")
        sys.exit(1)

    try:
        if operation == 'rd':
            current_setting = read_gpio(gpio_num)
            print(f"Current setting of GPIO {gpio_num}: {current_setting}")
        elif operation == 'wr':
            if len(sys.argv) != 5:
                print("Usage for write operation: python gpio_control.py <gpio_number> wr <setting_value> <duration>")
                sys.exit(1)
            setting = int(sys.argv[3])
            duration = int(sys.argv[4])
            if setting not in (0, 1):
                print("Setting value must be 0 or 1")
                sys.exit(1)
            write_gpio(gpio_num, setting, duration)
    except Exception as e:
        print(f"Error occurred: {e}")


# controller_V2

# used https://docs.python.org/3/library/socket.html to aid writing socket and udp
# and https://docs.python.org/3/library/tkinter.html for tkinter gui generation

import socket
import time
import threading
import tkinter as tk
from tkinter import ttk

# network communication
# check IP address that pico prints in serial
PICO_IP = "192.168.73.31"  
PICO_PORT = 4242

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

CONTROLLER_PORT = 4243
sock.bind(("", CONTROLLER_PORT))
sock.settimeout(0.5)

print(f"Python controller listening on UDP port {CONTROLLER_PORT}")

running = True
last_sent = None

# GUI button presses change these values
forward = 0
turn = 0

SPEEDS = {
    1: 80,   # low: for final alignment
    2: 90,   # medium: when approaching rock
    3: 100,  # high: general traversal
}

current_speed_level = 2
current_speed = SPEEDS[current_speed_level]

speed_buttons = {}

# UDP communications
# for repeated motion command sending, force=True
# so allows for repeated sending for those specific commands
# to mitigate effects of packet loss
def send_command(cmd, force=False):
    global last_sent

    if force or cmd != last_sent:
        sock.sendto(cmd.encode(), (PICO_IP, PICO_PORT))
        last_sent = cmd
        print(f"Sent: {cmd}")

# receiving messages in bg thread
# GUI is updated with root.after() so main thread updates tkinter
def receive_loop():
    while running:
        try:
            data, addr = sock.recvfrom(1024)
            msg = data.decode(errors="ignore").strip()

            print(f"From Pico {addr}: {msg}")

            root.after(0, handle_pico_message, msg)

        except socket.timeout:
            continue

        except OSError:
            break

        except Exception as e:
            print(f"Receive error: {e}")
            break


# motion button functionality
active_motion = None
motion_thread_running = False

# formatting string for command
def make_motion_command(forward, turn):
    return f"M {forward} {turn} {current_speed}"

# repeatedly sending commands if button held down
def motion_hold_loop():
    global motion_thread_running

    while motion_thread_running and active_motion is not None:
        forward, turn = active_motion
        send_command(make_motion_command(forward, turn), force=True)
        time.sleep(0.05)

def start_motion(forward, turn):
    global active_motion, motion_thread_running

    active_motion = (forward, turn)
    motion_thread_running = True

    threading.Thread(
        target=motion_hold_loop,
        daemon=True
    ).start()

def stop_motion():
    global active_motion, motion_thread_running

    active_motion = None
    motion_thread_running = False

    for _ in range(5):
        send_command("M 0 0 0", force=True)
        time.sleep(0.02)


# sensor command functionality
def send_sensor_command(command):
    send_command(command, force=True)

# safety output if app closes
def on_close():
    global running, active_motion, motion_thread_running

    running = False
    active_motion = None
    motion_thread_running = False

    # Safety commands so Pico does not continue last movement
    send_command("M 0 0 0", force=True)
    send_command("M 0 0 0", force=True)
    send_command("M 0 0 0", force=True)
    send_command("M 0 0 0", force=True)
    send_command("M 0 0 0", force=True)

    try:
        sock.close()
    except OSError:
        pass

    root.destroy()


# facilitating dual motion control - screen and keyboard
KEY_TO_MOTION = {
    "w": (1, 0),
    "s": (-1, 0),
    "a": (0, -1),
    "d": (0, 1),
}

pressed_keys = set()

def on_key_press(event):
    key = event.keysym.lower()

    # handling directional commands
    if key in KEY_TO_MOTION and key not in pressed_keys:
        pressed_keys.add(key)
        forward, turn = KEY_TO_MOTION[key]
        start_motion(forward, turn)
    # handling speed changing
    elif key == "1":
        set_speed_level(1)
    elif key == "2":
        set_speed_level(2)
    elif key == "3":
        set_speed_level(3)

def on_key_release(event):
    key = event.keysym.lower()

    if key in KEY_TO_MOTION:
        pressed_keys.discard(key)

        if pressed_keys:
            last_key = next(reversed(list(pressed_keys)))
            forward, turn = KEY_TO_MOTION[last_key]
            start_motion(forward, turn)
        else:
            stop_motion()



# Controller-side rock classification
UNKNOWN = "UNKNOWN"
classification_table = [
    "Gravion",    # 000
    UNKNOWN,      # 001
    UNKNOWN,      # 010
    "Regolix",    # 011
    UNKNOWN,      # 100
    "Lunarite",   # 101
    "Basaltoid",  # 110
    UNKNOWN       # 111
]
measured_pulse = None
measured_ultrasound = None
measured_magnetism = None


def classify_rock_client_side():
    global measured_pulse, measured_ultrasound, measured_magnetism

    if measured_pulse is None:
        rocktype_value.config(text="Need IR")
        return

    if measured_ultrasound is None:
        rocktype_value.config(text="Need ultrasound")
        return

    if measured_magnetism is None:
        rocktype_value.config(text="Need magnetic")
        return

    # convert 3 binary results into lookup index
    index = ((measured_pulse << 2) | (measured_ultrasound << 1) | measured_magnetism)

    rock_type = classification_table[index]
    rocktype_value.config(text=rock_type)
    # terminal output for confirmation
    print(
        f"Rock classification: pulse={measured_pulse}, "
        f"ultrasound={measured_ultrasound}, "
        f"magnetism={measured_magnetism}, "
        f"index={index}, rock={rock_type}"
    )


# clearing data
def reset_rock_data():
    global measured_pulse, measured_ultrasound, measured_magnetism

    measured_pulse = None
    measured_ultrasound = None
    measured_magnetism = None

    ir_value.config(text="312/547")
    ultrasound_value.config(text="40kHz/0")
    magnetic_value.config(text="Up/Down")
    age_value.config(text="#xxx")
    rocktype_value.config(text="Basaltoid/Gravion/\nRegolix/Lunarite")

    print("Rock data reset")



# speed changing on GUI
def update_speed_label():
    names = {
        1: "Low - alignment",
        2: "Medium - approach",
        3: "High - traversal",
    }

    speed_label.config(
        text=f"Speed {current_speed_level}: {names[current_speed_level]}"
    )

    for level, button in speed_buttons.items():
        if level == current_speed_level:
            button.config(bg="lightgreen", activebackground="lightgreen")
        else:
            button.config(bg="SystemButtonFace", activebackground="SystemButtonFace")


def set_speed_level(level):
    global current_speed_level, current_speed
   
    # safety during testing if values were wrong
    if level not in SPEEDS:
        return

    current_speed_level = level
    current_speed = SPEEDS[level]
    update_speed_label()
    print(f"Speed set to level {level}: {current_speed}%")



# TKinter GUI generation

root = tk.Tk()
root.title("Controller")
root.geometry("1050x650")
root.configure(bg="white")
root.protocol("WM_DELETE_WINDOW", on_close)

root.bind("<KeyPress>", on_key_press)
root.bind("<KeyRelease>", on_key_release)
root.focus_set()

# screen setup
style = ttk.Style()
style.theme_use("default")

style.configure("TButton", font=("Arial", 14), padding=10)

style.configure("Sensor.TButton", font=("Arial", 12), padding=8)

style.configure("Title.TLabel", font=("Arial", 26), background="white")

style.configure("Value.TLabel", font=("Arial", 14), background="white", relief="solid", padding=10)

title = ttk.Label(root, text="CONTROLLER", style="Title.TLabel")
title.pack(pady=20)

main_frame = tk.Frame(root, bg="white")
main_frame.pack(fill="both", expand=True, padx=20, pady=10)

motion_frame = tk.LabelFrame(main_frame, text="Motion Control", font=("Arial", 16), bg="white", padx=20, pady=15)
motion_frame.pack(side="left", fill="both", expand=True, padx=20)

sensor_frame = tk.LabelFrame(main_frame, text="Sensor Control", font=("Arial", 16), bg="white", padx=20, pady=15
)
sensor_frame.pack(side="right", fill="both", expand=True, padx=20)


# motion control button setup
motion_frame.configure(width=430, height=430)
motion_frame.pack_propagate(False)

motion_grid = tk.Frame(motion_frame, bg="white")
motion_grid.place(relx=0.5, rely=0.35, anchor="center") # relative placement helps across different screen sizes

BUTTON_W = 11
BUTTON_H = 3
PAD_X = 18
PAD_Y = 18

forward_button = tk.Button(motion_grid, text="Forward", font=("Arial", 15), width=BUTTON_W, height=BUTTON_H)
forward_button.grid(row=0, column=1, padx=PAD_X, pady=PAD_Y)

left_button = tk.Button(motion_grid, text="Turn\nLeft", font=("Arial", 15), width=BUTTON_W, height=BUTTON_H)
left_button.grid(row=1, column=0, padx=PAD_X, pady=PAD_Y)

right_button = tk.Button(motion_grid, text="Turn\nRight", font=("Arial", 15), width=BUTTON_W, height=BUTTON_H)
right_button.grid(row=1, column=2, padx=PAD_X, pady=PAD_Y)

backward_button = tk.Button(motion_grid, text="Backward", font=("Arial", 15), width=BUTTON_W, height=BUTTON_H)
backward_button.grid(row=2, column=1, padx=PAD_X, pady=PAD_Y)

# Press and hold behavior
forward_button.bind("<ButtonPress-1>", lambda e: start_motion(1, 0))
forward_button.bind("<ButtonRelease-1>", lambda e: stop_motion())

backward_button.bind("<ButtonPress-1>", lambda e: start_motion(-1, 0))
backward_button.bind("<ButtonRelease-1>", lambda e: stop_motion())

left_button.bind("<ButtonPress-1>", lambda e: start_motion(0, -1))
left_button.bind("<ButtonRelease-1>", lambda e: stop_motion())

right_button.bind("<ButtonPress-1>", lambda e: start_motion(0, 1))
right_button.bind("<ButtonRelease-1>", lambda e: stop_motion())


# speed setup
speed_panel = tk.Frame(motion_frame, bg="white")
speed_panel.place(relx=0.5, rely=0.88, anchor="center")

speed_label = tk.Label(speed_panel, text="", font=("Arial", 11), bg="white")
speed_label.grid(row=0, column=0, columnspan=3, pady=(0, 8))

speed_buttons[1] = tk.Button(speed_panel, text="1 Low\nAlign", width=10, command=lambda: set_speed_level(1))
speed_buttons[1].grid(row=1, column=0, padx=5)

speed_buttons[2] = tk.Button(speed_panel, text="2 Medium\nApproach", width=10, command=lambda: set_speed_level(2))
speed_buttons[2].grid(row=1, column=1, padx=5)

speed_buttons[3] = tk.Button(speed_panel, text="3 High\nTravel", width=10, command=lambda: set_speed_level(3))
speed_buttons[3].grid(row=1, column=2, padx=5)

update_speed_label()

# sensor control buttons
sensor_values = {}

sensor_rows = [
    ("ultrasound", "Measure\nUltrasound", "40kHz/0", "S ULTRASOUND"),
    ("ir", "Measure\nIR", "312/547", "S IR"),
    ("magnetic", "Measure\nMagnetic", "Up/Down", "S MAGNETIC"),
    ("age", "Measure\nAge", "#xxx", "S AGE"),
    ("rocktype", "Get rock\ntype", "Basaltoid/Gravion/\nRegolix/Lunarite", "CLIENT_CLASSIFY"),
]

# generating the table
for row, (key, button_text, value_text, command) in enumerate(sensor_rows):
    btn = ttk.Button(sensor_frame, text=button_text, style="Sensor.TButton",
        command=lambda c=command: (
            classify_rock_client_side() if c == "CLIENT_CLASSIFY" # because this is now client side so dont transmit
            else send_sensor_command(c)
        )
    )
    btn.grid(row=row, column=0, padx=15, pady=8, sticky="ew")

    value = ttk.Label(
        sensor_frame,
        text=value_text,
        style="Value.TLabel",
        anchor="center"
    )
    value.grid(row=row, column=1, padx=15, pady=8)

    sensor_values[key] = value

ultrasound_value = sensor_values["ultrasound"]
ir_value = sensor_values["ir"]
magnetic_value = sensor_values["magnetic"]
age_value = sensor_values["age"]
rocktype_value = sensor_values["rocktype"]

# reset button that takes up both columns
reset_button = ttk.Button(sensor_frame, text="Clear / Reset\nRock Data", style="Sensor.TButton", command=reset_rock_data)
reset_button.grid(row=len(sensor_rows), column=0, columnspan=2, padx=15, pady=8, sticky="ew")




# starting background threads for communication
threading.Thread(target=receive_loop, daemon=True).start()

print("Tkinter controller started")
print("Movement is controlled by GUI button holds and W, A, S, D for directions/1, 2, 3 for speed.")
print("Releasing a motion button sends neutral motion: M 0 0")


# decoding message sent back by pico
def handle_pico_message(msg):
    global measured_pulse, measured_ultrasound, measured_magnetism

    if msg.startswith("ULTRASOUND "):
        value = msg.replace("ULTRASOUND ", "", 1)
        ultrasound_value.config(text=value)

        if value == "0" or value == "NONE":
            measured_ultrasound = 0
        elif value == "40kHz":
            measured_ultrasound = 1
        elif value == "WEAK": # currenly not used in classification
            measured_ultrasound = None

    elif msg.startswith("IR "):
        value = msg.replace("IR ", "", 1)
        ir_value.config(text=value)

        if value == "312":
            measured_pulse = 0
        elif value == "547":
            measured_pulse = 1

    elif msg.startswith("MAGNETIC "):
        value = msg.replace("MAGNETIC ", "", 1)
        magnetic_value.config(text=value)

        if value == "DOWN":
            measured_magnetism = 0
        elif value == "UP":
            measured_magnetism = 1
        elif value == "IDLE":
            measured_magnetism = None

    elif msg.startswith("AGE "):
        age_value.config(text=msg.replace("AGE ", "", 1))

    elif msg.startswith("ROCKTYPE "):
        rocktype_value.config(text=msg.replace("ROCKTYPE ", "", 1))

    elif msg == "AGE STARTED":
        age_value.config(text="Listening...")

    else:
        print(f"Unhandled Pico message: {msg}")

# checking responsiveness and bidirectional comms
root.after(1000, lambda: send_command("PING", force=True))

root.mainloop()

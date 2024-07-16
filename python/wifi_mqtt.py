import tkinter as tk
from tkinter import ttk
from PIL import Image, ImageTk, ImageEnhance
import paho.mqtt.client as mqtt
import json
import threading
import signal
import os

# MQTT settings
MQTT_BROKER = "test.mosquitto.org"
MQTT_PORT = 1883
MQTT_TOPIC = "z/workshop/data"

# Global dictionary to hold the temperature data for each name
temp_data = {}
temp_blocks = {}

# Constants for block size and padding
BLOCK_SIZE = 170
PADDING = 5
BLOCKS_PER_ROW = 10
MAX_TEMP = 100  # Maximum temperature for scaling the thermometer

def update_temp_blocks():
    for name, temp in temp_data.items():
        if name not in temp_blocks:
            row, col = divmod(len(temp_blocks), BLOCKS_PER_ROW)
            frame = tk.Frame(root, width=BLOCK_SIZE, height=BLOCK_SIZE, bg="lightblue", borderwidth=1, relief="solid")
            frame.grid_propagate(False)

            name_label = tk.Label(frame, text=name, font=("Helvetica", 12), bg="lightblue", width=15, anchor="center")
            name_label.place(relx=0.5, y=10, anchor="n")

            canvas = tk.Canvas(frame, width=20, height=80, bg="white", bd=0, highlightthickness=0, relief="ridge")
            canvas.place(relx=0.5, rely=0.5, anchor="center")
            fill_height = (temp / MAX_TEMP) * 80
            canvas.create_rectangle(0, 80 - fill_height, 20, 80, fill="red")

            temp_label = tk.Label(frame, text=f"{temp}°C", font=("Helvetica", 12), bg="lightblue")
            temp_label.place(relx=0.5, rely=0.85, anchor="n")

            frame.place(x=col * (BLOCK_SIZE + PADDING), y=row * (BLOCK_SIZE + PADDING))
            temp_blocks[name] = (name_label, canvas, temp_label)
        else:
            name_label, canvas, temp_label = temp_blocks[name]
            name_label.config(text=name)
            temp_label.config(text=f"{temp}°C")
            canvas.delete("all")
            fill_height = (temp / MAX_TEMP) * 80
            canvas.create_rectangle(0, 80 - fill_height, 20, 80, fill="red")

def on_message(client, userdata, msg):
    global temp_data
    try:
        payload = json.loads(msg.payload.decode())
        name = payload.get("name")
        temp = payload.get("temp")
        if name and temp is not None:
            temp_data[name] = temp
            update_temp_blocks()
    except json.JSONDecodeError as e:
        print(f"JSON decode error: {e}")
    except Exception as e:
        print(f"Error processing message: {e}")

def setup_mqtt_client():
    client = mqtt.Client()
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.subscribe(MQTT_TOPIC)
    return client

def signal_handler(sig, frame):
    print("Exiting gracefully...")
    root.quit()
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
    exit(0)

def resize_image(event):
    canvas_width = event.width
    canvas_height = event.height

    new_width = canvas_width // 2
    new_height = int(new_width * original_logo_image.height / original_logo_image.width)

    if new_height > canvas_height // 2:
        new_height = canvas_height // 2
        new_width = int(new_height * original_logo_image.width / original_logo_image.height)

    resized_image = original_logo_image.resize((new_width, new_height), Image.LANCZOS)
    logo_photo = ImageTk.PhotoImage(resized_image)
    logo_canvas.create_image(canvas_width // 2, canvas_height // 2, image=logo_photo, anchor='center')
    logo_canvas.image = logo_photo

def start_application():
    global root, temp_blocks, logo_canvas, original_logo_image
    root = tk.Tk()
    root.title("Temperature Tracker")

    total_width = (BLOCK_SIZE + 2 * PADDING) * BLOCKS_PER_ROW
    root.geometry(f"{total_width}x600")
    root.configure(background="#ffe0e0")

    logo_canvas = tk.Canvas(root, bg="#ffe0e0")
    logo_canvas.pack(fill=tk.BOTH, expand=True)
    logo_canvas.bind('<Configure>', resize_image)

    # Load and place the background logo
    directory_path = os.path.dirname(__file__)
    file_path = os.path.join(directory_path, 'espressif.png')

    original_logo_image = Image.open(file_path)
    original_logo_image = original_logo_image.convert("RGBA")
    alpha = original_logo_image.split()[3]
    alpha = ImageEnhance.Brightness(alpha).enhance(0.4)
    original_logo_image.putalpha(alpha)

    temp_blocks = {}

    mqtt_client.loop_start()
    root.mainloop()

if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)

    mqtt_client = setup_mqtt_client()

    start_application()

import tkinter as tk
from tkinter import ttk, messagebox
import json
import serial
import serial.tools.list_ports
import time

# Initialize serial object
ser = None

def get_available_ports():
	"""Get a list of available COM ports."""
	ports = [port.device for port in serial.tools.list_ports.comports()]
	return ports

def connect_serial():
	"""Connect to the selected COM port."""
	global ser
	port = com_port_combo.get()
	try:
		ser = serial.Serial(port, 115200, timeout=1)
		status_label.config(text="Status: Connected")
	except Exception as e:
		messagebox.showerror("Error", f"Cannot connect to {port}. {e}")

def disconnect_serial():
	"""Disconnect from the COM port."""
	global ser
	if ser and ser.is_open:
		ser.close()
		status_label.config(text="Status: Disconnected")

def send_freq_to_powercore():
    # Retrieve the Pulse Frequency value from the GUI
    pwm_freq_value = freq_entry.get()
    
    # Format the data into the custom delimited format
    data_string = f"pwm_frequency={pwm_freq_value}"
    
    # Send the data over the serial connection
    if ser.is_open:
        ser.write(data_string.encode('utf-8'))


def send_coulomb_to_powercore():
    # Retrieve the Max µC per pulse value from the GUI
    micro_coulomb_value = max_coulomb_entry.get()
    
    # Format the data into the custom delimited format
    data_string = f"micro_c_per_pulse={micro_coulomb_value}"
    
    # Send the data over the serial connection
    if ser.is_open:
        ser.write(data_string.encode('utf-8'))

def read_from_serial():
	"""
	Read data from the serial port, parse it, and update the GUI.
	"""
	global ser
	data_dict = {}  # Initialize the dictionary
	if ser and ser.is_open:
		try:
			# Read a line from the serial port
			data_string = ser.readline().decode().strip()
			
			# Parse the custom delimited format
			#data_dict = {pair.split('=')[0]: pair.split('=')[1] for pair in data_string.split(',')}
			data_items = data_string.split(',')
			for item in data_items:
				if '=' in item:
					key, value = item.split('=')
					data_dict[key.strip()] = value.strip()			
			
			# Update GUI components conditionally
			if "spark%" in data_dict:
				spark_progress["value"] = int(data_dict["spark%"])
			if "short%" in data_dict:
				short_progress["value"] = int(data_dict["short%"])
			if "avgPower" in data_dict:
				avg_power_value["text"] = data_dict["avgPower"]
			if "avgCharge" in data_dict:
				avg_charge_value["text"] = data_dict["avgCharge"]
			if "pulseFreq" in data_dict:
				freq_value["text"] = data_dict["pulseFreq"]
			if "maxCoulomb" in data_dict:
				max_coulomb_value["text"] = data_dict["maxCoulomb"]
			if "mosfetTemp" in data_dict:
				mosfetTemp["text"] = data_dict["mosfetTemp"]
			if "resistorTemp" in data_dict:
				resistorTemp["text"] = data_dict["resistorTemp"]
			# Append the message to the message box
			message = data_dict.get("message", "")
			if message:  # Only append if there's a new message
				message_box.insert(tk.END, f"\n[{time.strftime('%H:%M:%S')}] {message}")
				message_box.see(tk.END)

		except Exception as e:
			print(f"Error reading/parsing serial data: {e}")
	# Schedule the read_from_serial function to start when the GUI is launched
	root.after(100, read_from_serial)

# Create the main window
root = tk.Tk()
root.title("Powercore EDM Controller")

# Schedule the read_from_serial function to start when the GUI is launched
root.after(100, read_from_serial)

# Frame for "Cut Status" section
cut_status_frame = ttk.LabelFrame(root, text="Cut Status")
cut_status_frame.pack(pady=10, padx=20, fill="x")

# Progress bars and labels for "Spark %" and "Short %"
spark_progress = ttk.Progressbar(cut_status_frame, orient="horizontal", length=200, mode="determinate")
spark_progress.grid(row=0, column=1, pady=5, padx=5)
spark_label = ttk.Label(cut_status_frame, text="Spark %")
spark_label.grid(row=0, column=0, pady=5, padx=5)

short_progress = ttk.Progressbar(cut_status_frame, orient="horizontal", length=200, mode="determinate")
short_progress.grid(row=1, column=1, pady=5, padx=5)
short_label = ttk.Label(cut_status_frame, text="Short %")
short_label.grid(row=1, column=0, pady=5, padx=5)

# Readouts for "Average Power" and "Average Spark Charge"
avg_power_label = ttk.Label(cut_status_frame, text="Average Spark Power (W):")
avg_power_label.grid(row=2, column=0, pady=5, padx=5)
avg_power_value = ttk.Label(cut_status_frame, text="0")
avg_power_value.grid(row=2, column=1, pady=5, padx=5)

avg_charge_label = ttk.Label(cut_status_frame, text="Average Spark Charge (µC):")
avg_charge_label.grid(row=3, column=0, pady=5, padx=5)
avg_charge_value = ttk.Label(cut_status_frame, text="0")
avg_charge_value.grid(row=3, column=1, pady=5, padx=5)

# MOSFET Temperature
mosfet_temp_label = ttk.Label(cut_status_frame, text="MOSFET Temperature (°C):")
mosfet_temp_label.grid(row=4, column=0, padx=10, pady=5, sticky=tk.W)

mosfetTemp = ttk.Label(cut_status_frame, text="0")  # Initialize with default value
mosfetTemp.grid(row=4, column=1, padx=10, pady=5)

# Power Resistor Temperature
resistor_temp_label = ttk.Label(cut_status_frame, text="Power Resistor Temperature (°C):")
resistor_temp_label.grid(row=5, column=0, padx=10, pady=5, sticky=tk.W)

resistorTemp = ttk.Label(cut_status_frame, text="0")  # Initialize with default value
resistorTemp.grid(row=5, column=1, padx=10, pady=5)


# Frame for "Control" section
control_frame = ttk.LabelFrame(root, text="Control")
control_frame.pack(pady=10, padx=20, fill="x")

# Headers for Current and Target
ttk.Label(control_frame, text="Current").grid(row=0, column=1, pady=5, padx=5)
ttk.Label(control_frame, text="Target").grid(row=0, column=2, pady=5, padx=5)

# Pulse Frequency
ttk.Label(control_frame, text="Pulse Frequency (Hz):").grid(row=1, column=0, pady=5, padx=5, sticky=tk.W)
freq_value = ttk.Label(control_frame, text="1500")
freq_value.grid(row=1, column=1, pady=5, padx=5)
freq_entry = ttk.Entry(control_frame)
freq_entry.grid(row=1, column=2, pady=5, padx=5)
update_freq_button = ttk.Button(control_frame, text="Update", command=send_freq_to_powercore)
update_freq_button.grid(row=1, column=3, pady=5, padx=5)

# Max micro coulomb per pulse
ttk.Label(control_frame, text="Max µC per pulse:").grid(row=2, column=0, pady=5, padx=5, sticky=tk.W)
max_coulomb_value = ttk.Label(control_frame, text="1600")
max_coulomb_value.grid(row=2, column=1, pady=5, padx=5)
max_coulomb_entry = ttk.Entry(control_frame)
max_coulomb_entry.insert(0, "1400")
max_coulomb_entry.grid(row=2, column=2, pady=5, padx=5)
update_coulomb_button = ttk.Button(control_frame, text="Update", command=send_coulomb_to_powercore)
update_coulomb_button.grid(row=2, column=3, pady=5, padx=5)


# Frame for "Connection" section
connection_frame = ttk.LabelFrame(root, text="Connection")
connection_frame.pack(pady=10, padx=20, fill="x")

# COM port selector, connect and disconnect buttons, and status label
com_port_label = ttk.Label(connection_frame, text="COM Port:")
com_port_label.grid(row=0, column=0, pady=5, padx=5)
com_port_combo = ttk.Combobox(connection_frame)
com_port_combo.grid(row=0, column=1, pady=5, padx=5)
connect_button = ttk.Button(connection_frame, text="Connect")
connect_button.grid(row=0, column=2, pady=5, padx=5)
disconnect_button = ttk.Button(connection_frame, text="Disconnect")
disconnect_button.grid(row=0, column=3, pady=5, padx=5)
status_label = ttk.Label(connection_frame, text="Status: Disconnected")
status_label.grid(row=1, column=0, columnspan=4, pady=5, padx=5)

# Frame for "Messages" section
messages_frame = ttk.LabelFrame(root, text="Messages")
messages_frame.pack(pady=10, padx=20, fill="both", expand=True)

# Text widget for the message box
message_box = tk.Text(messages_frame, height=4, width=50)  # Adjust height and width as needed
message_box.pack(pady=10, padx=10, fill="both", expand=True)


# Update COM port dropdown with available ports
com_port_combo["values"] = get_available_ports()

# Connect buttons to their actions
connect_button.config(command=connect_serial)
disconnect_button.config(command=disconnect_serial)


root.mainloop()




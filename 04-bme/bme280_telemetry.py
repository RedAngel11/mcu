import time
import serial
import matplotlib.pyplot as plt

def read_value(ser):
    while True:
        try:
            line = ser.readline().decode('ascii').strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) == 3:
                temp = float(parts[0])
                pres = float(parts[1])
                hum = float(parts[2])
                return temp, pres, hum
        except (ValueError, UnicodeDecodeError):
            continue

def main():
    port = 'COM7'
    ser = serial.Serial(port=port, baudrate=115200, timeout=0.0)
    
    if ser.is_open:
        print(f"Port {ser.name} opened")
    
    timestamps = []
    temperatures = []
    pressures = []
    humidities = []
    
    start_time = time.time()
    
    try:
        ser.write("bme_start\n".encode('ascii'))
        print("BME280 telemetry started")
        print("Press Ctrl+C to stop")
        
        while True:
            current_time = time.time() - start_time
            temp, pres, hum = read_value(ser)
            
            timestamps.append(current_time)
            temperatures.append(temp)
            pressures.append(pres / 100.0)
            humidities.append(hum)
            
            print(f"[{current_time:5.1f}s] T={temp:6.2f}°C, "
                  f"P={pres/100:7.2f}hPa, H={hum:5.1f}%")
            
    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        ser.write("bme_stop\n".encode('ascii'))
        ser.close()
        
        plt.figure(figsize=(12, 8))
        
        plt.subplot(3,1,1)
        plt.plot(timestamps, temperatures, 'r-')
        plt.title('Temperature')
        plt.ylabel('°C')
        plt.grid(True)
        
        plt.subplot(3,1,2)
        plt.plot(timestamps, pressures, 'b-')
        plt.title('Pressure')
        plt.ylabel('hPa')
        plt.grid(True)
        
        plt.subplot(3,1,3)
        plt.plot(timestamps, humidities, 'g-')
        plt.title('Humidity')
        plt.xlabel('Time (s)')
        plt.ylabel('%')
        plt.grid(True)
        
        plt.tight_layout()
        plt.show()

if __name__ == "__main__":
    main()
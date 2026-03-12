import time
import serial
import matplotlib.pyplot as plt


def read_value(ser):
    while True:
        try:
            line = ser.readline().decode('ascii').strip()
            value = float(line)
            return value
        except ValueError:
            continue

def main():
    ser = serial.Serial(port='COM6', baudrate=115200, timeout=0.0)
    
    if ser.is_open:
        print(f"Port {ser.name} opened")
    else:
        print(f"Port {ser.name} closed")
        return
    
    measures_temperature_C = []
    measures_voltage_V = []
    measures_ts = []
    
    start_ts = time.time()
    
    try:
        while True:
            ts = time.time() - start_ts
            
            ser.write("get_adc\n".encode('ascii'))
            voltage_V = read_value(ser)
            
            ser.write("get_temp\n".encode('ascii'))
            temp_C = read_value(ser)
            
            measures_ts.append(ts)
            measures_voltage_V.append(voltage_V)
            measures_temperature_C.append(temp_C)
            
            print(f'{voltage_V:.3f} V - {temp_C:.1f}C - {ts:.2f}s')
            
            time.sleep(0.1)
            
    except KeyboardInterrupt:
        print("\nMeasurement stopped by user")
    finally:
        ser.close()
        print("Port closed")
        
        plt.figure(figsize=(10, 8))
        
        plt.subplot(2, 1, 1)
        plt.plot(measures_ts, measures_voltage_V, 'b-', linewidth=1.5)
        plt.title('График зависимости напряжения от времени')
        plt.xlabel('время, с')
        plt.ylabel('напряжение, В')
        plt.grid(True, alpha=0.3)
        
        plt.subplot(2, 1, 2)
        plt.plot(measures_ts, measures_temperature_C, 'r-', linewidth=1.5)
        plt.title('График зависимости температуры от времени')
        plt.xlabel('время, с')
        plt.ylabel('температура, °C')
        plt.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.show()

if __name__ == "__main__":
    main()
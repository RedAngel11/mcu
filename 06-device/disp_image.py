import time
import serial
from PIL import Image

SCREEN_WIDTH = 320   
SCREEN_HEIGHT = 240
COM_PORT = 'COM8'
BAUDRATE = 115200

def main():
    ser = None
    try:
        # Открываем порт
        ser = serial.Serial(port=COM_PORT, baudrate=BAUDRATE, timeout=1)
        time.sleep(0.5)
        
        # грузим изображение
        image = Image.open('C:/Repositories/pico/mcu/05-display/get2.jpg')
        width, height = image.size
        print(f"Original image size: {width}x{height}")
        
        # === размер фото под экран ===
        if width != SCREEN_WIDTH or height != SCREEN_HEIGHT:
            image = image.resize((SCREEN_WIDTH, SCREEN_HEIGHT), Image.Resampling.LANCZOS)
            width, height = SCREEN_WIDTH, SCREEN_HEIGHT
            print(f"Resized to: {width}x{height}")
        
        # черный фон
        ser.write("disp_screen 000000\n".encode('ascii'))
        time.sleep(0.1)
        
        # Отправляем пиксели
        print(f"Sending {width * height} pixels...")
        start_time = time.time()
        
        for y in range(height):
            for x in range(width):
                r, g, b = image.getpixel((x, y))
                color = (r << 16) | (g << 8) | b
                ser.write(f"disp_px {x} {y} {color:06X}\n".encode('ascii'))
            
            if y % 10 == 0:
                print(f"Progress: {y}/{height} rows ({100*y//height}%)")
        
        elapsed = time.time() - start_time
        print(f"Done! Transferred in {elapsed:.2f}s ({width*height/elapsed:.0f} px/s)")
        
    except FileNotFoundError:
        print("Error: Image file not found!")
    except serial.SerialException as e:
        print(f"Error: Could not open port {COM_PORT}: {e}")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        time.sleep(0.1)
        if ser and ser.is_open:
            ser.close()
            print("Serial port closed.")

if __name__ == "__main__":
    main()
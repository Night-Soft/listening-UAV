import serial
import struct
import sys
import time
from colorama import Fore, Back, Style, init

# Обязательно вызовите init() для корректной работы в Windows
init(autoreset=True)

def printB(text):
    print(Fore.BLUE + text)

# замените порт на свой: Windows -> "COM3", Linux/Mac -> "/dev/ttyUSB0"
PORT = "/dev/ttyUSB0"
BAUD = 230400 #576000

ser = serial.Serial(PORT, BAUD)

spectr = []

def writeSpectr():
    global spectr, ser
    data = ser.read(8)  # читаем бинарные данные с ESP32
    value = struct.unpack("<d", data)[0]
    spectr.append(value)

def stopWriteAudio():
    global ser
    printB("Send stop")
    printB("Waiting stop...")
    
    time.sleep(0.1)
    ser.write(b"stop\n")  # отправляем подтверждение
    ser.flush()

def getSpectr():
    printB("Send start")
    ser.write(b"getNoiseSpectr\n")
    recording = False

    try:
        while True:
            line = ser.readline().decode(errors="ignore").strip()

            if line == "SendSpectr":
                printB("▶ Spectr writing...")
                print(Fore.YELLOW + "▶ Нажмите ctrl+c для остановки")
                recording = True

            if line == "Record stop":
                printB("⏹ Остановка записи.")
                recording = False
                break  # выходим из цикла и закрываем файл

            # если запись активна — сохраняем данные
            if recording:
               # time.sleep(0.5)
                ser.reset_input_buffer()

                for i in range(128):
                    writeSpectr()
                    print(Fore.YELLOW + f"\rWriting: {i}s  ", end="")

                    #print(Fore.YELLOW + f"\spectr: {0}  ", end="")

                recording = False
                print("\n")
                break
                #stopWriteAudio()
                #continue

    except KeyboardInterrupt:
        printB("\nПрервано пользователем.")
        stopWriteAudio()

pathPython = "/home/User/Documents/PlatformIO/Projects/Listening UAV/python/"
pathWav = "/home/User/Documents/PlatformIO/Projects/Listening UAV/python/wav/"
pathSrc = "/home/User/Documents/PlatformIO/Projects/Listening UAV/src/reference/"

def createFile(samples, varName = "silenceSpectr"):
    global pathSrc
    define = "SILENCE_SPECTR_H"
    outFile = f"{pathSrc}{varName}.h"
    with open(outFile, "w") as f:
        f.write(f"#ifndef {define}\n")
        f.write(f"#define {define}\n\n")

        f.write(f"const double {varName}[{len(samples)}] = {{")
        f.write(", ".join(map(str, samples)))
        f.write("};\n")

        f.write("\n#endif\n")

    print(f"✅ Создан файл {outFile} с {len(samples)} значениями")

getSpectr()
createFile(samples = spectr,varName = "silenceSpectr")
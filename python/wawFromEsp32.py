import serial
import wave
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

channels = 1
sample_width = 2        # 16 бит
frame_rate = 8000       # Гц
duration = 10
num_samples = frame_rate * duration
samples = []
infinityRecording = False

if len(sys.argv) > 1:
    if sys.argv[1] == "inf":
        infinityRecording = True
    else:
        try:
            duration = int(sys.argv[1])
            num_samples = frame_rate * duration

        except ValueError:
            printB("Invalid input: cannot convert to integer")
fisrstLast = []
isFirstSet = False
isLastSet = False
def writeAudio(wf):
    global samples, ser, fisrstLast, isFirstSet, isLastSet
    data = ser.read(2)  # читаем бинарные данные с ESP32
    if isFirstSet == False:
        value = struct.unpack("<h", data)[0]
        fisrstLast.append(value)
        isFirstSet = True

    wf.writeframes(data)

def stopWriteAudio():
    global ser
    printB("Send stop")
    printB("Waiting stop...")
    
    time.sleep(0.1)
    ser.write(b"stop\n")  # отправляем подтверждение
    ser.flush()

with wave.open("output.wav", "wb") as wf:
    wf.setnchannels(channels)
    wf.setsampwidth(sample_width)
    wf.setframerate(frame_rate)

    recording = False
    printB("Send start")
    ser.write(b"recordToPc\n")

    try:
        while True:
            line = ser.readline().decode(errors="ignore").strip()

            if line == "Record start":
                if infinityRecording:
                    printB("▶ Начало записи...")
                    print(Fore.YELLOW + "▶ Нажмите ctrl+c для остановки")
                else: 
                    printB(f"▶ Начало записи {duration} секунд")
                    
                recording = True
                continue

            if line == "Record stop":
                printB("⏹ Остановка записи.")
                recording = False
                break  # выходим из цикла и закрываем файл

            # если запись активна — сохраняем данные
            if recording:
                time.sleep(0.5)
                ser.reset_input_buffer()

                if infinityRecording == False:
                    counter = 0
                    seconds = 0
                    print(Fore.YELLOW + f"\rWriting: {0}s  ", end="")
                    for i in range(num_samples):
                        writeAudio(wf)
                        counter +=1
                        if counter % 8000 == 0:
                            seconds +=1
                            print(Fore.YELLOW + f"\rWriting: {seconds}s  ", end="")

                    recording = False
                    print("\n")
                    stopWriteAudio()
                    continue

                try:
                    while True:
                        writeAudio(wf)
                except KeyboardInterrupt:
                    printB("\nПрервано пользователем.")
                    recording = False
                    stopWriteAudio()
                

    except KeyboardInterrupt:
        printB("\nПрервано пользователем.")
        stopWriteAudio()



# with open("data uint16_t.txt", "w") as f:
#     f.write(", ".join(map(str, samples)))

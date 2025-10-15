import serial
import wave
import struct


# замените порт на свой: Windows -> "COM3", Linux/Mac -> "/dev/ttyUSB0"
PORT = "/dev/ttyUSB0"
BAUD = 576000

ser = serial.Serial(PORT, BAUD)

channels = 1
sample_width = 2        # 16 бит
frame_rate = 8000       # Гц
duration = 10
num_samples = frame_rate * duration
samples = []
infinityRecording = False


def writeAudio(wf):
    global samples, ser
    data = ser.read(2)  # читаем бинарные данные с ESP32
    #value = struct.unpack("<h", data)[0]
    #samples.append(value)
    wf.writeframes(data)

def stopWriteAudio():
    global ser
    print("Send stop")
    ser.write(b"stop\n")  # отправляем подтверждение
    print("Waiting stop...")

with wave.open("output.wav", "wb") as wf:
    wf.setnchannels(channels)
    wf.setsampwidth(sample_width)
    wf.setframerate(frame_rate)

    recording = False
    print("Send start")
    ser.write(b"start test\n")  # отправляем подтверждение

    try:
          #for i in range(num_samples):
            #writeAudio(wf)
        while True:
            line = ser.readline().decode(errors="ignore").strip()

            if line == "Record start":
                print("▶ Начало записи...")
                recording = True
                continue

            if line == "Record stop":
                print("⏹ Остановка записи.")
                recording = False
                continue  
            if line == "Test send":
                print("▶ ⏹ Test send.")
                continue  

    except KeyboardInterrupt:
        print("\nПрервано пользователем.")
        stopWriteAudio()


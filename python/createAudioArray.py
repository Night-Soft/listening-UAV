import wave
import struct

def extract_audio_segment(wav_filename, start_sec=0, duration_sec=1):
    # Открываем WAV
    wav_file = wave.open(wav_filename, "rb")

    channels = wav_file.getnchannels()
    sample_width = wav_file.getsampwidth()
    frame_rate = wav_file.getframerate()
    n_frames = wav_file.getnframes()

    print(f"Channels: {channels}, Sample Width: {sample_width}, Frame Rate: {frame_rate}, Total Frames: {n_frames}")

    # Вычисляем кадры для чтения
    start_frame = int(start_sec * frame_rate)
    frames_to_read = int(duration_sec * frame_rate)

    # Перемещаемся к нужному кадру
    wav_file.setpos(start_frame)

    # Читаем данные
    frames = wav_file.readframes(frames_to_read)

    audio_data = struct.unpack("<" + "h"*frames_to_read*channels, frames)

    # Преобразуем байты в массив чисел
    #if sample_width == 2:  # 16-bit PCM
    #    audio_data = struct.unpack("<" + "h"*frames_to_read*channels, frames)
    #elif sample_width == 1:  # 8-bit PCM
    #    audio_data = struct.unpack("<" + "B"*frames_to_read*channels, frames)

    wav_file.close()

    # Для стерео оставляем только первый канал
    if channels == 2:
        audio_data = audio_data[::2]

    return audio_data

# Пример использования
start_time = 0  # начинаем с 5-й секунды
segment = extract_audio_segment("full-reference 1.wav", start_sec=start_time, duration_sec=1)

# Генерация массива для ESP32
with open("testRef.h", "w") as f:
    f.write("#ifndef AUDIO_ARRAY\n")
    f.write("#define AUDIO_ARRAY\n\n")
    f.write("#include <Arduino.h>\n\n")
    f.write("int16_t referenceAudio[8000] = {")
    f.write(", ".join(map(str, segment)))
    f.write("};\n\n")
    f.write("#endif\n")
    #f.write(f"const int audio_len = {len(segment)};\n")

print(f"Segment from {start_time}s saved, length = {len(segment)} samples")

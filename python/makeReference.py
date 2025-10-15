counter = 0

def make_reference_audio(in_file, out_file, start_index=0, count=8000, varName="referenceAudio"):
    global counter
    counter +=1
    # читаем все числа из файла
    with open(in_file, "r") as f:
        content = f.read()

    # преобразуем в список int
    numbers = [int(x.strip()) for x in content.split(",") if x.strip()]

    # берем нужный диапазон
    slice_numbers = numbers[start_index:start_index + count]

    # формируем .h файл
    with open(out_file, "w") as f:
        f.write(f"#ifndef AUDIO_ARRAY{counter}\n")
        f.write(f"#define AUDIO_ARRAY{counter}\n\n")
        f.write("#include <Arduino.h>\n\n")

        if varName == "referenceAudio":
            f.write(f"const int16_t {varName}{counter}[{len(slice_numbers)}] = {{")
        else:
            f.write(f"const int16_t {varName}[{len(slice_numbers)}] = {{")

        f.write(", ".join(map(str, slice_numbers)))
        f.write("};\n\n")
        f.write("#endif\n")

    print(f"✅ Создан файл {out_file} с {len(slice_numbers)} значениями")

# берём 4000 чисел начиная с индекса 10000
pathPython = "/home/User/Documents/PlatformIO/Projects/Listening UAV/python/"
pathSrc = "/home/User/Documents/PlatformIO/Projects/Listening UAV/src/reference/"
# make_reference_audio(
#     f"{pathPython}uint16_t reference 1.txt",
#     f"{pathSrc}audioSamples1.h",
#     start_index=58240,
#     count=4096
#     )
# make_reference_audio(
#     f"{pathPython}uint16_t reference 2.txt",
#     f"{pathSrc}audioSamples2.h",
#     start_index=115880,
#     count=4096
#     )
# make_reference_audio(
#     f"{pathPython}uint16_t reference 1.txt",
#     f"{pathPython}testReff1.h",
#     start_index=0,
#     count=50
#     )

# uav approaching
make_reference_audio(
    f"{pathPython}uint16_t reference 3-4.txt",
    f"{pathSrc}uavApproachingt.h",
    start_index=406010,
    count=4096,
    varName="uavApproachingt"
    )

# # uav moves away
# make_reference_audio(
#     f"{pathPython}uint16_t reference 3-4.txt",
#     f"{pathSrc}uavMovesAway.h",
#     start_index=757872,
#     count=4096,
#     varName="uavMovesAway"
#     )
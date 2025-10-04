def make_reference_audio(in_file, out_file, start_index=0, count=8000):
    # читаем все числа из файла
    with open(in_file, "r") as f:
        content = f.read()

    # преобразуем в список int
    numbers = [int(x.strip()) for x in content.split(",") if x.strip()]

    # берем нужный диапазон
    slice_numbers = numbers[start_index:start_index + count]

    # формируем .h файл
    with open(out_file, "w") as f:
        f.write("#ifndef AUDIO_ARRAY\n")
        f.write("#define AUDIO_ARRAY\n\n")
        f.write("#include <Arduino.h>\n\n")
        f.write(f"int16_t referenceAudio[{len(slice_numbers)}] = {{")
        f.write(", ".join(map(str, slice_numbers)))
        f.write("};\n\n")
        f.write("#endif\n")

    print(f"✅ Создан файл {out_file} с {len(slice_numbers)} значениями")

# берём 4000 чисел начиная с индекса 10000
make_reference_audio("data uint16_t.txt", "referenceAudio.h", start_index=55999, count=8000)

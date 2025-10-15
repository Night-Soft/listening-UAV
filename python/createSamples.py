import wave
import struct

def extractSegment(wawFilename, startSmp=0, count=1024, numSamples=4, distanceBtwSampl=1024): #128ms = 1024samples
    # Открываем WAV
    wav_file = wave.open(wawFilename, "rb")

    channels = wav_file.getnchannels()
    sample_width = wav_file.getsampwidth()
    frame_rate = wav_file.getframerate()
    n_frames = wav_file.getnframes()

    print(f"Channels: {channels}, Sample Width: {sample_width}, Frame Rate: {frame_rate}, Total Frames: {n_frames}")

    samples = []
    for i in range(numSamples):   # от 0 до 4
        if i > 0:
            start_frame = int(startSmp) + distanceBtwSampl
        else:
            start_frame = int(startSmp)

        frames_to_read = int(count)

        wav_file.setpos(start_frame)
        frames = wav_file.readframes(frames_to_read)

        audioData = struct.unpack("<" + "h"*frames_to_read*channels, frames)
        samples.extend(audioData)  

    wav_file.close()

    return samples

pathPython = "/home/User/Documents/PlatformIO/Projects/Listening UAV/python/"
pathSrc = "/home/User/Documents/PlatformIO/Projects/Listening UAV/src/reference/"

def createFile(samples, varName = "test"):
    global pathSrc
    outFile = f"{pathSrc}{varName}.h"
    define = createDefine(varName)

    with open(outFile, "w") as f:
        f.write(f"#ifndef {define}\n")
        f.write(f"#define {define}\n\n")
        f.write("#include <Arduino.h>\n\n")

        f.write(f"const int16_t {varName}[{len(samples)}] = {{")

        f.write(", ".join(map(str, samples)))
        f.write("};\n\n")
        f.write("#endif\n")

    print(f"✅ Создан файл {outFile} с {len(samples)} значениями")

def createDefine(name = "tEstAud"):
    letters = []
    for letter in name:
        if letter.isupper():
            letters.append("_")
        
        letters.append(letter)    
    
    letters.append("_H")
    header = "".join(letters).upper()
    print(header)
    return header

def createSamplesH(
        inFile, startPos=0, count=1024, numSamples=4,
        distanceBtwSampl=3072, varName="referenceAudio"
        ):
    
    samples = extractSegment(
        inFile,
        startSmp=startPos, 
        count=count, 
        numSamples=numSamples,
        distanceBtwSampl=distanceBtwSampl
        )
    createFile(samples, varName)

# createSamplesH(
#     f"{pathPython}output full new.wav",
#     varName="startRef",
#     startPos=793920,
#     count=1024,
#     numSamples=4,
#     distanceBtwSampl=4096
# )

# createSamplesH(
#     f"{pathPython}output full new.wav",
#     varName="midddleRef1",
#     startPos=1188000,
#     count=1024,
#     numSamples=4,
#     distanceBtwSampl=4096
# )

# createSamplesH(
#     f"{pathPython}output full new.wav",
#     varName="midddleRef2",
#     startPos=3548160,
#     count=1024,
#     numSamples=4,
#     distanceBtwSampl=12096
# )

# createSamplesH(
#     f"{pathPython}output full new.wav",
#     varName="awayRef",
#     startPos=6739968,
#     count=1024,
#     numSamples=4,
#     distanceBtwSampl=20096
# )

# createSamplesH(
#     f"{pathPython}output full new.wav",
#     varName="awayRef2",
#     startPos=6641600,
#     count=1024,
#     numSamples=4,
#     distanceBtwSampl=8000
# )

# createSamplesH(
#     f"{pathPython}full-reference 3-4.wav",
#     varName="awayRef",
#     startPos=747000,
#     count=1024,
#     numSamples=4,
#     distanceBtwSampl=5120
# )

# createSamplesH(
#     f"{pathPython}full-reference 3-4.wav",
#     varName="awayRef",
#     startPos=74740,
#     count=1024,
#     numSamples=4,
#     distanceBtwSampl=5120
# )

createSamplesH(
    f"{pathPython}full-reference 3-4.wav",
    varName="startRef2",
    startPos=481600,
    count=1024,
    numSamples=4,
    distanceBtwSampl=5120
)
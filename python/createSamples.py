import wave
import struct

def extractSegment(
        wawFilename,
        startSmp=0,
        count=1024, 
        numSamples=4, 
        distanceBtwSampl=1024): #128ms = 1024samples
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
pathWav = "/home/User/Documents/PlatformIO/Projects/Listening UAV/python/wav/"
pathSrc = "/home/User/Documents/PlatformIO/Projects/Listening UAV/src/reference/"

def createFile(samples, varName = "test", hzRange=[445], js=False):
    global pathSrc
    define = createDefine(varName)

    outFile = ""
    if js == True:
        outFile = f"{pathSrc}{varName}.js"
        with open(outFile, "w") as f:
            f.write(f"const {varName} = [")
            f.write(", ".join(map(str, samples)))
            f.write("];\n")
    else:
        outFile = f"{pathSrc}{varName}.h"
        with open(outFile, "w") as f:
            f.write(f"#ifndef {define}\n")
            f.write(f"#define {define}\n\n")
            f.write("#include <Arduino.h>\n\n")

            f.write(f"const int16_t {varName}[{len(samples)}] = {{")
            f.write(", ".join(map(str, samples)))
            f.write("};\n")

            f.write(f"const int16_t hzRange_{varName}[{len(hzRange)}] = {{")
            f.write(", ".join(map(str, hzRange)))
            f.write("};\n")

            f.write("\n#endif\n")

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
        distanceBtwSampl=3072, varName="referenceAudio",hzRange=[445], js=False
        ):
    
    samples = extractSegment(
        inFile,
        startSmp=startPos, 
        count=count, 
        numSamples=numSamples,
        distanceBtwSampl=distanceBtwSampl
        )
    createFile(samples, varName, hzRange, js)

# fileName = "output01.11.25.wav"
fileName = "output 04.11.25.wav"
createSamplesH(
    f"{pathPython}{fileName}",
    varName="startRef217_10_25",
    startPos=95520,
    count=1024,
    numSamples=4,
    distanceBtwSampl=0,
    hzRange = [670, 1500]
)

createSamplesH(
    f"{pathPython}{fileName}",
    varName="midddleRef117_10_25",
    startPos=236400,
    count=1024,
    numSamples=4,
    distanceBtwSampl=0,
    hzRange = [640, 1500]
)

createSamplesH(
    f"{pathPython}{fileName}",
    varName="midddleRef217_10_25",
    startPos=546948,
    count=1024,
    numSamples=4,
    distanceBtwSampl=0,
    hzRange = [400, 1500]
)

createSamplesH(
    f"{pathPython}{fileName}",
    varName="awayRef17_10_25",
    startPos=507408,
    count=1024,
    numSamples=4,
    distanceBtwSampl=0,
    hzRange = [470, 1360]

)

# js

# pathSrc = pathPython
# noiseSilence = 'noise silence.wav'
# createSamplesH(
#     f"{pathWav}{noiseSilence}",
#     varName="noiseSilence",
#     startPos=0,
#     count=40000,
#     numSamples=1,
#     distanceBtwSampl=0,
#     js=True,
#     hzRange = [0, 4000]
# )
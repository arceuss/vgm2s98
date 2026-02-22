#include "s98_writer.h"
#include <string.h>
#include <algorithm>

S98Writer::S98Writer() : file(NULL), dataStartOffset(0), loopOffset(0), 
                         tagOffset(0), currentDataPos(0), loopSet(false), nextDeviceId(0) {
}

S98Writer::~S98Writer() {
    Close();
}

bool S98Writer::Open(const char* filename) {
    Close();
    
    file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    // Write placeholder header (will be finalized later)
    WriteHeader();
    dataStartOffset = ftell(file);
    currentDataPos = 0;
    
    return true;
}

void S98Writer::Close() {
    if (file) {
        Finalize();
        fclose(file);
        file = NULL;
    }
    devices.clear();
    deviceIdMap.clear();
    dataStartOffset = 0;
    loopOffset = 0;
    tagOffset = 0;
    currentDataPos = 0;
    loopSet = false;
    nextDeviceId = 0;
}

void S98Writer::AddDevice(S98DeviceType type, uint32_t clock, uint32_t pan) {
    // Check if device already exists
    for (size_t i = 0; i < devices.size(); i++) {
        if (devices[i].type == type) {
            return; // Already added
        }
    }
    
    S98Device dev;
    dev.type = type;
    dev.clock = clock;
    dev.pan = pan;
    dev.deviceId = nextDeviceId;
    
    devices.push_back(dev);
    deviceIdMap[type] = nextDeviceId;
    
    // Device IDs are assigned in pairs (even numbers)
    nextDeviceId += 2;
}

void S98Writer::WriteWait(uint32_t ticks) {
    if (!file) return;
    
    if (ticks == 0) {
        return;
    } else if (ticks == 1) {
        WriteUint8(0xFF); // 1 tick
        currentDataPos++;
    } else {
        WriteUint8(0xFE); // Multiple ticks
        currentDataPos++;
        uint32_t count = ticks - 2; // S98 encoding: n ticks = 0xFE + (n-2)
        size_t varIntSize = WriteVarInt(count);
        currentDataPos += varIntSize;
    }
}

void S98Writer::WriteRegister(uint8_t deviceId, uint8_t reg, uint8_t data) {
    if (!file) return;
    
    WriteUint8(deviceId);
    WriteUint8(reg);
    WriteUint8(data);
    currentDataPos += 3;
}

void S98Writer::WriteEnd() {
    if (!file) return;
    
    WriteUint8(0xFD); // End marker
    currentDataPos++;
}

void S98Writer::SetLoopPoint() {
    if (!file || loopSet) return;
    
    loopOffset = currentDataPos;
    loopSet = true;
}

void S98Writer::WriteTag(const std::map<std::string, std::string>& tags) {
    if (!file) return;
    
    tagOffset = ftell(file);
    
    // Write S98 v3 tag format: [S98] followed by UTF-8 BOM, then key=value pairs
    fwrite("[S98]", 1, 5, file);
    
    // Write UTF-8 BOM for proper encoding detection
    WriteUint8(0xEF);
    WriteUint8(0xBB);
    WriteUint8(0xBF);
    
    for (const auto& pair : tags) {
        std::string line = pair.first + "=" + pair.second + "\n";
        fwrite(line.c_str(), 1, line.length(), file);
    }
    
    // Null terminator
    WriteUint8(0);
}

void S98Writer::Finalize() {
    if (!file) return;
    
    uint32_t currentFilePos = ftell(file);
    
    // Write header with correct offsets
    fseek(file, 0, SEEK_SET);
    WriteHeader();
    
    // Update offsets in header
    fseek(file, 0x14, SEEK_SET); // dataOfs offset
    WriteUint32(dataStartOffset);
    
    fseek(file, 0x18, SEEK_SET); // loopOfs offset
    if (loopSet && loopOffset > 0) {
        WriteUint32(dataStartOffset + loopOffset);
    } else {
        WriteUint32(0);
    }
    
    fseek(file, 0x10, SEEK_SET); // tagOfs offset
    if (tagOffset > 0) {
        WriteUint32(tagOffset);
    } else {
        WriteUint32(0);
    }
    
    fseek(file, currentFilePos, SEEK_SET);
}

void S98Writer::WriteHeader() {
    if (!file) return;
    
    // Write S98 v3 header
    fwrite("S983", 1, 4, file); // Magic + version
    
    WriteUint32(1);  // timerNumerator
    WriteUint32(44100); // timerDenominator (samples per second)
    WriteUint32(0);  // compression (always 0)
    WriteUint32(0);  // tagOfs (will be updated in Finalize)
    WriteUint32(0);  // dataOfs (will be updated in Finalize)
    WriteUint32(0);  // loopOfs (will be updated in Finalize)
    WriteUint32((uint32_t)devices.size()); // deviceCount
    
    // Write device info (0x20 +)
    for (const auto& dev : devices) {
        WriteUint32((uint32_t)dev.type);
        WriteUint32(dev.clock);
        WriteUint32(dev.pan);
        WriteUint32(0); // reserved
    }
    
    // Pad to 0x20 if no devices (for v0/v1 compatibility)
    if (devices.empty()) {
        // Write default device (OPNA)
        WriteUint32((uint32_t)S98_DEV_OPNA);
        WriteUint32(7987200);
        WriteUint32(0);
        WriteUint32(0);
    }
}

uint8_t S98Writer::GetDeviceId(S98DeviceType type) const {
    auto it = deviceIdMap.find(type);
    if (it != deviceIdMap.end()) {
        return it->second;
    }
    return 0xFF; // Invalid
}

void S98Writer::WriteUint8(uint8_t value) {
    if (file) {
        fwrite(&value, 1, 1, file);
    }
}

void S98Writer::WriteUint16(uint16_t value) {
    if (file) {
        fwrite(&value, 1, 2, file);
    }
}

void S98Writer::WriteUint32(uint32_t value) {
    if (file) {
        fwrite(&value, 1, 4, file);
    }
}

size_t S98Writer::WriteVarInt(uint32_t value) {
    // Variable-length integer encoding (similar to MIDI)
    // Each byte: bit 7 = continue, bits 0-6 = data
    size_t bytesWritten = 0;
    uint32_t v = value;
    
    while (v >= 0x80) {
        WriteUint8(0x80 | (v & 0x7F));
        bytesWritten++;
        v >>= 7;
    }
    WriteUint8(v & 0x7F);
    bytesWritten++;
    
    return bytesWritten;
}

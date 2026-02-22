#ifndef S98_WRITER_H
#define S98_WRITER_H

#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <map>

// S98 device types (from s98device.h)
enum S98DeviceType {
    S98_DEV_PSG = 1,
    S98_DEV_OPN = 2,
    S98_DEV_OPN2 = 3,
    S98_DEV_OPNA = 4,
    S98_DEV_OPM = 5,
    S98_DEV_OPLL = 6,
    S98_DEV_OPL = 7,
    S98_DEV_OPL2 = 8,
    S98_DEV_OPL3 = 9,
    S98_DEV_MSXA = 0x0A,
    S98_DEV_SNG = 0x10,
    S98_DEV_AY8910 = 15,
    S98_DEV_SN76489 = 16,
    S98_DEV_NONE = 0
};

struct S98Device {
    S98DeviceType type;
    uint32_t clock;
    uint32_t pan;
    uint8_t deviceId; // Internal ID for this device in the S98 file
    
    S98Device() : type(S98_DEV_NONE), clock(0), pan(0), deviceId(0) {}
};

// S98 Writer class
class S98Writer {
public:
    S98Writer();
    ~S98Writer();
    
    bool Open(const char* filename);
    void Close();
    
    // Add device (call before writing data)
    void AddDevice(S98DeviceType type, uint32_t clock, uint32_t pan = 0);
    
    // Write commands
    void WriteWait(uint32_t ticks); // ticks = samples (S98 uses 1:1 with samples at 44100Hz)
    void WriteRegister(uint8_t deviceId, uint8_t reg, uint8_t data);
    void WriteEnd();
    
    // Set loop point (call when reaching the loop point in VGM)
    void SetLoopPoint();
    
    // Write tag data (S98 v3 format)
    void WriteTag(const std::map<std::string, std::string>& tags);
    
    // Finalize file (write header with correct offsets)
    void Finalize();
    
    bool IsOpen() const { return file != NULL; }
    
    // Get device ID for a chip type
    uint8_t GetDeviceId(S98DeviceType type) const;
    
private:
    FILE* file;
    std::vector<S98Device> devices;
    uint32_t dataStartOffset;
    uint32_t loopOffset;
    uint32_t tagOffset;
    uint32_t currentDataPos;
    bool loopSet;
    
    std::map<S98DeviceType, uint8_t> deviceIdMap;
    uint8_t nextDeviceId;
    
    void WriteUint8(uint8_t value);
    void WriteUint16(uint16_t value);
    void WriteUint32(uint32_t value);
    void WriteHeader();
    size_t WriteVarInt(uint32_t value); // Variable-length integer encoding
};

#endif // S98_WRITER_H

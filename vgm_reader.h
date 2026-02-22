#ifndef VGM_READER_H
#define VGM_READER_H

#include <stdint.h>
#include <stdio.h>
#include <vector>
#include <string>

// VGM command types
enum VGMCommandType {
    VGM_CMD_WAIT = 0x61,
    VGM_CMD_WAIT_735 = 0x62,  // 60Hz wait
    VGM_CMD_WAIT_882 = 0x63,  // 50Hz wait
    VGM_CMD_WAIT_SHORT = 0x70, // 0x70-0x7F: wait 1-16 samples
    VGM_CMD_END = 0x66,
    
    // Chip write commands
    VGM_CMD_SN76489 = 0x50,
    VGM_CMD_YM2413 = 0x51,
    VGM_CMD_YM2612_PORT0 = 0x52,
    VGM_CMD_YM2612_PORT1 = 0x53,
    VGM_CMD_YM2151 = 0x54,
    VGM_CMD_YM2203 = 0x55,
    VGM_CMD_YM2608_PORT0 = 0x56,
    VGM_CMD_YM2608_PORT1 = 0x57,
    VGM_CMD_YM2610_PORT0 = 0x58,
    VGM_CMD_YM2610_PORT1 = 0x59,
    VGM_CMD_YM3812 = 0x5A,
    VGM_CMD_YM3526 = 0x5B,
    VGM_CMD_AY8910 = 0xA0,
    
    // Data block
    VGM_CMD_DATA_BLOCK = 0x67,
    
    // PCM seek
    VGM_CMD_PCM_SEEK = 0xE0,
};

struct VGMCommand {
    uint8_t cmd;
    uint32_t waitSamples;  // For wait commands
    uint8_t reg;           // For register writes
    uint8_t data;          // For register writes
    uint8_t port;          // For chips with ports (0 or 1)
    uint32_t blockType;    // For data blocks
    uint32_t blockSize;    // For data blocks
    std::vector<uint8_t> blockData; // For data blocks
    uint32_t pcmOffset;    // For PCM seek
    
    VGMCommand() : cmd(0), waitSamples(0), reg(0), data(0), port(0), 
                   blockType(0), blockSize(0), pcmOffset(0) {}
};

struct VGMHeader {
    uint32_t version;
    uint32_t eofOffset;
    uint32_t totalSamples;
    uint32_t loopOffset;
    uint32_t loopSamples;
    uint32_t dataOffset;
    uint32_t gd3Offset;
    
    // Chip clocks
    uint32_t sn76489Clock;
    uint32_t ym2413Clock;
    uint32_t ym2612Clock;
    uint32_t ym2151Clock;
    uint32_t ym2203Clock;
    uint32_t ym2608Clock;
    uint32_t ym2610Clock;
    uint32_t ym3812Clock;
    uint32_t ym3526Clock;
    uint32_t ay8910Clock;

    // Volume modifier (VGM 1.60+, offset 0x7C)
    // Volume = 2 ^ (volumeModifier / 32.0); default 0 => factor 1.0
    int8_t volumeModifier;
    
    VGMHeader() : version(0), eofOffset(0), totalSamples(0), loopOffset(0),
                  loopSamples(0), dataOffset(0), gd3Offset(0),
                  sn76489Clock(0), ym2413Clock(0), ym2612Clock(0),
                  ym2151Clock(0), ym2203Clock(0), ym2608Clock(0),
                  ym2610Clock(0), ym3812Clock(0), ym3526Clock(0), ay8910Clock(0),
                  volumeModifier(0) {}
};

// VGM Reader class
class VGMReader {
public:
    VGMReader();
    ~VGMReader();
    
    bool Open(const char* filename);
    void Close();
    
    bool ReadHeader(VGMHeader& header);
    bool ReadNextCommand(VGMCommand& cmd);
    void Reset(); // Reset to start of data
    
    bool IsOpen() const { return file != NULL; }
    uint32_t GetCurrentPosition() const { return currentPos; }
    uint32_t GetDataStartOffset() const { return dataStartOffset; }
    uint32_t GetLoopOffset() const { return loopOffset; }
    
private:
    FILE* file;
    VGMHeader header;
    uint32_t dataStartOffset;
    uint32_t loopOffset;
    uint32_t currentPos;
    uint32_t fileSize;
    
    uint8_t ReadUint8();
    uint16_t ReadUint16();
    uint32_t ReadUint32();
    void Seek(uint32_t offset);
};

#endif // VGM_READER_H

#include "vgm_reader.h"
#include <string.h>
#include <stdlib.h>

VGMReader::VGMReader() : file(NULL), dataStartOffset(0), loopOffset(0), currentPos(0), fileSize(0) {
}

VGMReader::~VGMReader() {
    Close();
}

bool VGMReader::Open(const char* filename) {
    Close();
    
    file = fopen(filename, "rb");
    if (!file) {
        return false;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    return true;
}

void VGMReader::Close() {
    if (file) {
        fclose(file);
        file = NULL;
    }
    dataStartOffset = 0;
    loopOffset = 0;
    currentPos = 0;
    fileSize = 0;
}

bool VGMReader::ReadHeader(VGMHeader& hdr) {
    if (!file) {
        return false;
    }
    
    fseek(file, 0, SEEK_SET);
    
    // Check magic
    char magic[4];
    if (fread(magic, 1, 4, file) != 4 || memcmp(magic, "Vgm ", 4) != 0) {
        return false;
    }
    
    hdr.eofOffset = ReadUint32();
    hdr.version = ReadUint32();
    hdr.sn76489Clock = ReadUint32();
    hdr.ym2413Clock = ReadUint32();
    hdr.gd3Offset = ReadUint32();
    hdr.totalSamples = ReadUint32();
    hdr.loopOffset = ReadUint32();
    hdr.loopSamples = ReadUint32();
    
    // Skip rate (0x24)
    Seek(0x24);
    uint32_t rate = ReadUint32();
    (void)rate;
    
    // Skip SN76489 flags (0x28-0x2B)
    Seek(0x2C);
    hdr.ym2612Clock = ReadUint32();
    hdr.ym2151Clock = ReadUint32();
    
    // VGM data offset (0x34)
    Seek(0x34);
    hdr.dataOffset = ReadUint32();
    if (hdr.dataOffset == 0) {
        hdr.dataOffset = 0x40; // Default for old VGM files
    } else {
        hdr.dataOffset = hdr.dataOffset + 0x34; // Relative offset from 0x34
    }
    
    // Skip Sega PCM (0x38-0x3F)
    Seek(0x40);
    hdr.ym2203Clock = ReadUint32();
    hdr.ym2608Clock = ReadUint32();
    hdr.ym2610Clock = ReadUint32();
    hdr.ym3812Clock = ReadUint32();
    hdr.ym3526Clock = ReadUint32();
    
    // Skip more chips (0x58-0x6F)
    Seek(0x74);
    hdr.ay8910Clock = ReadUint32();

    // Volume Modifier (VGM 1.60+, offset 0x7C)
    // Spec says players should support it in v1.50+ files too.
    if (hdr.version >= 0x150 && fileSize > 0x7C) {
        Seek(0x7C);
        hdr.volumeModifier = (int8_t)ReadUint8();
    } else {
        hdr.volumeModifier = 0;
    }
    
    // Store offsets
    dataStartOffset = hdr.dataOffset;
    if (hdr.loopOffset > 0) {
        loopOffset = hdr.loopOffset + 0x1C; // Relative to 0x1C
    } else {
        loopOffset = 0;
    }
    
    // Seek to data start
    Seek(dataStartOffset);
    currentPos = dataStartOffset;
    
    header = hdr;
    return true;
}

bool VGMReader::ReadNextCommand(VGMCommand& cmd) {
    if (!file || currentPos >= fileSize) {
        return false;
    }
    
    fseek(file, currentPos, SEEK_SET);
    uint8_t byte = ReadUint8();
    currentPos++;
    
    cmd = VGMCommand();
    cmd.cmd = byte;
    
    if (byte == VGM_CMD_END) {
        // End of data
        return true;
    } else if (byte == VGM_CMD_WAIT) {
        // Wait n samples (0x61 nn nn)
        uint16_t samples = ReadUint16();
        currentPos += 2;
        cmd.waitSamples = samples;
    } else if (byte == VGM_CMD_WAIT_735) {
        // Wait 735 samples (60Hz)
        cmd.waitSamples = 735;
    } else if (byte == VGM_CMD_WAIT_882) {
        // Wait 882 samples (50Hz)
        cmd.waitSamples = 882;
    } else if (byte >= VGM_CMD_WAIT_SHORT && byte <= 0x7F) {
        // Short wait: 0x70-0x7F = wait 1-16 samples
        cmd.waitSamples = (byte - VGM_CMD_WAIT_SHORT) + 1;
    } else if (byte == VGM_CMD_DATA_BLOCK) {
        // Data block: 0x67 0x66 tt ss ss ss ss [data]
        uint8_t marker = ReadUint8();
        currentPos++;
        if (marker == 0x66) {
            cmd.blockType = ReadUint8();
            currentPos++;
            cmd.blockSize = ReadUint32();
            currentPos += 4;
            
            // Read block data
            cmd.blockData.resize(cmd.blockSize);
            if (fread(cmd.blockData.data(), 1, cmd.blockSize, file) == cmd.blockSize) {
                currentPos += cmd.blockSize;
            }
        }
    } else if (byte == VGM_CMD_PCM_SEEK) {
        // PCM seek: 0xE0 oo oo oo oo
        cmd.pcmOffset = ReadUint32();
        currentPos += 4;
    } else if (byte == VGM_CMD_SN76489 || byte == VGM_CMD_YM2413 || 
               byte == VGM_CMD_YM2612_PORT0 || byte == VGM_CMD_YM2612_PORT1 ||
               byte == VGM_CMD_YM2151 || byte == VGM_CMD_YM2203 ||
               byte == VGM_CMD_YM2608_PORT0 || byte == VGM_CMD_YM2608_PORT1 ||
               byte == VGM_CMD_YM2610_PORT0 || byte == VGM_CMD_YM2610_PORT1 ||
               byte == VGM_CMD_YM3812 || byte == VGM_CMD_YM3526 ||
               byte == VGM_CMD_AY8910) {
        // Register write: cmd reg data
        cmd.reg = ReadUint8();
        cmd.data = ReadUint8();
        currentPos += 2;
        
        // Determine port
        if (byte == VGM_CMD_YM2612_PORT1 || byte == VGM_CMD_YM2608_PORT1 || 
            byte == VGM_CMD_YM2610_PORT1) {
            cmd.port = 1;
        } else {
            cmd.port = 0;
        }
    } else {
        // Unknown command - skip it
        fprintf(stderr, "Warning: Unknown VGM command 0x%02X at offset 0x%X\n", byte, currentPos - 1);
    }
    
    return true;
}

void VGMReader::Reset() {
    if (file && dataStartOffset > 0) {
        Seek(dataStartOffset);
        currentPos = dataStartOffset;
    }
}

uint8_t VGMReader::ReadUint8() {
    uint8_t value = 0;
    if (file) {
        fread(&value, 1, 1, file);
    }
    return value;
}

uint16_t VGMReader::ReadUint16() {
    uint16_t value = 0;
    if (file) {
        fread(&value, 1, 2, file);
    }
    return value;
}

uint32_t VGMReader::ReadUint32() {
    uint32_t value = 0;
    if (file) {
        fread(&value, 1, 4, file);
    }
    return value;
}

void VGMReader::Seek(uint32_t offset) {
    if (file) {
        fseek(file, offset, SEEK_SET);
    }
}

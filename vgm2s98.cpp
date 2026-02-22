#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <string>
#include <vector>
#include <map>
#include "vgm_reader.h"
#include "s98_writer.h"

// Map VGM chip commands to S98 device types
S98DeviceType GetS98DeviceType(uint8_t vgmCmd) {
    switch (vgmCmd) {
        case VGM_CMD_SN76489:
            return S98_DEV_SN76489;
        case VGM_CMD_YM2203:
            return S98_DEV_OPN;
        case VGM_CMD_YM2612_PORT0:
        case VGM_CMD_YM2612_PORT1:
            return S98_DEV_OPN2;
        case VGM_CMD_YM2608_PORT0:
        case VGM_CMD_YM2608_PORT1:
            return S98_DEV_OPNA;
        case VGM_CMD_YM2151:
            return S98_DEV_OPM;
        case VGM_CMD_YM2413:
            return S98_DEV_OPLL;
        case VGM_CMD_YM3812:
            return S98_DEV_OPL;
        case VGM_CMD_YM3526:
            return S98_DEV_OPL2;
        case VGM_CMD_AY8910:
            return S98_DEV_AY8910;
        default:
            return S98_DEV_NONE;
    }
}

uint32_t GetVGMClock(uint8_t vgmCmd, const VGMHeader& header) {
    switch (vgmCmd) {
        case 0x50: // SN76489
            return header.sn76489Clock;
        case 0x55: // YM2203
            return header.ym2203Clock;
        case 0x52: // YM2612_PORT0
        case 0x53: // YM2612_PORT1
            return header.ym2612Clock;
        case 0x56: // YM2608_PORT0
        case 0x57: // YM2608_PORT1
            return header.ym2608Clock;
        case 0x54: // YM2151
            return header.ym2151Clock;
        case 0x51: // YM2413
            return header.ym2413Clock;
        case 0x5A: // YM3812
            return header.ym3812Clock;
        case 0x5B: // YM3526
            return header.ym3526Clock;
        case 0xA0: // AY8910
            return header.ay8910Clock;
        default:
            return 0;
    }
}

// Extract GD3 tag metadata from VGM file
bool ExtractGD3Tags(const char* vgmFilename, std::map<std::string, std::string>& tags) {
    FILE* f = fopen(vgmFilename, "rb");
    if (!f) return false;
    
    // Read header
    fseek(f, 0, SEEK_SET);
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "Vgm ", 4) != 0) {
        fclose(f);
        return false;
    }
    
    fseek(f, 0x14, SEEK_SET);
    uint32_t gd3Offset = 0;
    fread(&gd3Offset, 1, 4, f);
    
    if (gd3Offset == 0) {
        fclose(f);
        return false;
    }
    
    // GD3 offset is relative to 0x14
    uint32_t gd3Pos = gd3Offset + 0x14;
    fseek(f, gd3Pos, SEEK_SET);
    
    // Check GD3 magic
    char gd3Magic[4];
    if (fread(gd3Magic, 1, 4, f) != 4 || memcmp(gd3Magic, "Gd3 ", 4) != 0) {
        fclose(f);
        return false;
    }
    
    // Read version and length
    uint32_t version, length;
    fread(&version, 1, 4, f);
    fread(&length, 1, 4, f);
    (void)version;
    (void)length;
    
    // Read UTF-16 strings (title, game, system, composer, release date, notes)
    // Each string is UTF-16LE, null-terminated
    auto ReadUTF16String = [&f](const char* fieldName = nullptr) -> std::string {
        std::vector<uint16_t> utf16;
        uint8_t bytes[2];
        bool firstChar = true;
        while (fread(bytes, 1, 2, f) == 2) {
            // Read as little-endian (low byte first)
            uint16_t ch = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
            
            // Skip BOM if present (0xFFFE for UTF-16LE, 0xFEFF for UTF-16BE)
            if (firstChar) {
                firstChar = false;
                if (ch == 0xFFFE || ch == 0xFEFF) {
                    continue;
                }
            }
            
            if (ch == 0) break;
            utf16.push_back(ch);
        }
        (void)fieldName;
        
        // Convert UTF-16LE to UTF-8
        std::string result;
        for (size_t i = 0; i < utf16.size(); i++) {
            uint32_t codePoint = utf16[i];
            
            // Handle surrogate pairs FIRST (before any other processing)
            if (codePoint >= 0xD800 && codePoint <= 0xDBFF && i + 1 < utf16.size()) {
                uint16_t low = utf16[i + 1];
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                    i++; // Skip the low surrogate
                }
            }
            
            // Convert fullwidth characters to ASCII equivalents (only for BMP characters)
            if (codePoint < 0x10000) {
                if (codePoint >= 0xFF01 && codePoint <= 0xFF5E) {
                    // Fullwidth ASCII variants -> normal ASCII (0xFF01-0xFF5E -> 0x0021-0x007E)
                    codePoint = codePoint - 0xFF00;
                } else if (codePoint >= 0xFFE0 && codePoint <= 0xFFE6) {
                    // Fullwidth currency symbols -> ASCII equivalents
                    if (codePoint == 0xFFE5) codePoint = 0x00A5; // Fullwidth yen -> yen sign
                    else if (codePoint == 0xFFE0) codePoint = 0x00A2; // Fullwidth cent -> cent sign
                    else if (codePoint == 0xFFE1) codePoint = 0x00A3; // Fullwidth pound -> pound sign
                    else if (codePoint == 0xFFE6) codePoint = 0x20A9; // Fullwidth won -> won sign
                }
            }
            
            // Convert code point to UTF-8
            if (codePoint < 0x80) {
                result += (char)codePoint;
            } else if (codePoint < 0x800) {
                result += (char)(0xC0 | (codePoint >> 6));
                result += (char)(0x80 | (codePoint & 0x3F));
            } else if (codePoint < 0x10000) {
                result += (char)(0xE0 | (codePoint >> 12));
                result += (char)(0x80 | ((codePoint >> 6) & 0x3F));
                result += (char)(0x80 | (codePoint & 0x3F));
            } else {
                result += (char)(0xF0 | (codePoint >> 18));
                result += (char)(0x80 | ((codePoint >> 12) & 0x3F));
                result += (char)(0x80 | ((codePoint >> 6) & 0x3F));
                result += (char)(0x80 | (codePoint & 0x3F));
            }
        }
        return result;
    };
    
    // GD3 format has 11 strings:
    // 1. Track Name (EN)
    // 2. Track Name (JP)
    // 3. Game Name (EN)
    // 4. Game Name (JP)
    // 5. System Name (EN)
    // 6. System Name (JP)
    // 7. Artist (EN)
    // 8. Artist (JP)
    // 9. Release Date
    // 10. VGM Creator
    // 11. Notes
    
    std::string titleEN = ReadUTF16String("titleEN");
    std::string titleJP = ReadUTF16String("titleJP");
    std::string gameEN = ReadUTF16String("gameEN");
    std::string gameJP = ReadUTF16String("gameJP");
    std::string systemEN = ReadUTF16String("systemEN");
    std::string systemJP = ReadUTF16String("systemJP");
    std::string artistEN = ReadUTF16String("artistEN");
    std::string artistJP = ReadUTF16String("artistJP");
    std::string releaseDate = ReadUTF16String("releaseDate");
    std::string vgmCreator = ReadUTF16String("vgmCreator");
    std::string notes = ReadUTF16String("notes");
    
    // Use English version if available, otherwise Japanese
    std::string title = !titleEN.empty() ? titleEN : titleJP;
    std::string game = !gameEN.empty() ? gameEN : gameJP;
    std::string system = !systemEN.empty() ? systemEN : systemJP;
    std::string composer = !artistEN.empty() ? artistEN : artistJP;
    
    if (!title.empty()) tags["title"] = title;
    if (!game.empty()) tags["game"] = game;
    if (!system.empty()) tags["system"] = system;
    if (!composer.empty()) tags["artist"] = composer;
    if (!releaseDate.empty()) tags["year"] = releaseDate;
    if (!vgmCreator.empty()) tags["s98by"] = vgmCreator;
    if (!notes.empty()) tags["comment"] = notes;
    
    fclose(f);
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.vgm> <output.s98>\n", argv[0]);
        return 1;
    }
    
    const char* inputFile = argv[1];
    const char* outputFile = argv[2];
    
    // Open VGM file
    VGMReader reader;
    if (!reader.Open(inputFile)) {
        fprintf(stderr, "Error: Could not open input file: %s\n", inputFile);
        return 1;
    }
    
    // Read VGM header
    VGMHeader vgmHeader;
    if (!reader.ReadHeader(vgmHeader)) {
        fprintf(stderr, "Error: Invalid VGM file: %s\n", inputFile);
        reader.Close();
        return 1;
    }
    
    fprintf(stderr, "VGM Version: %d.%02d\n", (vgmHeader.version >> 8) & 0xFF, vgmHeader.version & 0xFF);
    fprintf(stderr, "Total samples: %u\n", vgmHeader.totalSamples);
    fprintf(stderr, "Loop samples: %u\n", vgmHeader.loopSamples);
    if (vgmHeader.volumeModifier != 0) {
        // Volume = 2 ^ (volumeModifier / 32.0)
        double gainFactor = pow(2.0, vgmHeader.volumeModifier / 32.0);
        fprintf(stderr, "Volume modifier: %d (gain factor: %.4f)\n",
                (int)vgmHeader.volumeModifier, gainFactor);
    }
    
    // Create S98 writer
    S98Writer writer;
    if (!writer.Open(outputFile)) {
        fprintf(stderr, "Error: Could not create output file: %s\n", outputFile);
        reader.Close();
        return 1;
    }
    
    // Add devices based on chips used in VGM
    // We'll discover devices as we parse commands, but add common ones first
    if (vgmHeader.ym2608Clock > 0) {
        writer.AddDevice(S98_DEV_OPNA, vgmHeader.ym2608Clock);
        fprintf(stderr, "Added YM2608 (OPNA) device, clock: %u Hz\n", vgmHeader.ym2608Clock);
    }
    if (vgmHeader.ym2612Clock > 0) {
        writer.AddDevice(S98_DEV_OPN2, vgmHeader.ym2612Clock);
        fprintf(stderr, "Added YM2612 (OPN2) device, clock: %u Hz\n", vgmHeader.ym2612Clock);
    }
    if (vgmHeader.ym2203Clock > 0) {
        writer.AddDevice(S98_DEV_OPN, vgmHeader.ym2203Clock);
        fprintf(stderr, "Added YM2203 (OPN) device, clock: %u Hz\n", vgmHeader.ym2203Clock);
    }
    if (vgmHeader.ym2151Clock > 0) {
        writer.AddDevice(S98_DEV_OPM, vgmHeader.ym2151Clock);
        fprintf(stderr, "Added YM2151 (OPM) device, clock: %u Hz\n", vgmHeader.ym2151Clock);
    }
    if (vgmHeader.ym2413Clock > 0) {
        writer.AddDevice(S98_DEV_OPLL, vgmHeader.ym2413Clock);
        fprintf(stderr, "Added YM2413 (OPLL) device, clock: %u Hz\n", vgmHeader.ym2413Clock);
    }
    if (vgmHeader.ym3812Clock > 0) {
        writer.AddDevice(S98_DEV_OPL, vgmHeader.ym3812Clock);
        fprintf(stderr, "Added YM3812 (OPL) device, clock: %u Hz\n", vgmHeader.ym3812Clock);
    }
    if (vgmHeader.ym3526Clock > 0) {
        writer.AddDevice(S98_DEV_OPL2, vgmHeader.ym3526Clock);
        fprintf(stderr, "Added YM3526 (OPL2) device, clock: %u Hz\n", vgmHeader.ym3526Clock);
    }
    if (vgmHeader.ay8910Clock > 0) {
        writer.AddDevice(S98_DEV_AY8910, vgmHeader.ay8910Clock);
        fprintf(stderr, "Added AY8910 device, clock: %u Hz\n", vgmHeader.ay8910Clock);
    }
    if (vgmHeader.sn76489Clock > 0) {
        writer.AddDevice(S98_DEV_SN76489, vgmHeader.sn76489Clock);
        fprintf(stderr, "Added SN76489 device, clock: %u Hz\n", vgmHeader.sn76489Clock);
    }
    
    // Convert VGM commands to S98
    uint32_t totalSamples = 0;
    uint32_t loopStartSamples = 0;
    bool atLoopPoint = false;
    VGMCommand cmd;
    
    // Calculate loop start position
    if (vgmHeader.loopSamples > 0) {
        if (vgmHeader.loopSamples == vgmHeader.totalSamples) {
            // Song loops from the start
            loopStartSamples = 0;
        } else {
            // Loop starts after intro
            loopStartSamples = vgmHeader.totalSamples - vgmHeader.loopSamples;
        }
        fprintf(stderr, "Loop will start at %u samples (loop length: %u samples)\n", 
                loopStartSamples, vgmHeader.loopSamples);
    }
    
    fprintf(stderr, "Converting VGM data to S98...\n");
    
    uint32_t regWriteCount = 0;
    uint32_t waitCount = 0;
    uint32_t unknownCount = 0;
    
    while (reader.ReadNextCommand(cmd)) {
        if (cmd.cmd == VGM_CMD_END) {
            writer.WriteEnd();
            break;
        }
        if (cmd.waitSamples > 0) {
            waitCount++;
            // Write wait command
            writer.WriteWait(cmd.waitSamples);
            totalSamples += cmd.waitSamples;
            
            // Check if we've reached the loop point
            if (!atLoopPoint && loopStartSamples > 0 && totalSamples >= loopStartSamples) {
                writer.SetLoopPoint();
                atLoopPoint = true;
                fprintf(stderr, "Loop point set at %u samples\n", totalSamples);
            } else if (!atLoopPoint && loopStartSamples == 0 && vgmHeader.loopSamples > 0) {
                // Loop from start - set immediately
                writer.SetLoopPoint();
                atLoopPoint = true;
                fprintf(stderr, "Loop point set at start (0 samples)\n");
            }
        }
        
        // Handle register writes (check by command type)
        if (cmd.cmd == 0x50 || cmd.cmd == 0x51 || // SN76489, YM2413
            cmd.cmd == 0x52 || cmd.cmd == 0x53 || // YM2612 port 0/1
            cmd.cmd == 0x54 || cmd.cmd == 0x55 || // YM2151, YM2203
            cmd.cmd == 0x56 || cmd.cmd == 0x57 || // YM2608 port 0/1
            cmd.cmd == 0x58 || cmd.cmd == 0x59 || // YM2610 port 0/1
            cmd.cmd == 0x5A || cmd.cmd == 0x5B || // YM3812, YM3526
            cmd.cmd == 0xA0) { // AY8910
            // Register write command
            S98DeviceType devType = GetS98DeviceType(cmd.cmd);
            
            if (devType != S98_DEV_NONE) {
                // Get or add device
                uint8_t deviceId = writer.GetDeviceId(devType);
                if (deviceId == 0xFF) {
                    // Device not added yet, add it now
                    uint32_t clock = GetVGMClock(cmd.cmd, vgmHeader);
                    if (clock == 0) {
                        // Default clock for PC98 YM2608
                        if (devType == S98_DEV_OPNA) {
                            clock = 8000000;
                        } else {
                            continue; // Skip if no clock info
                        }
                    }
                    writer.AddDevice(devType, clock);
                    deviceId = writer.GetDeviceId(devType);
                }
                
                // S98 format: device ID is base (even) + port (0 or 1)
                uint8_t s98DeviceId = deviceId + cmd.port;
                
                writer.WriteRegister(s98DeviceId, cmd.reg, cmd.data);
                regWriteCount++;
            }
        } else if (cmd.cmd == VGM_CMD_DATA_BLOCK) {
            // Data blocks are not directly supported in S98
            fprintf(stderr, "Skipping data block type 0x%02X\n", cmd.blockType);
        } else if (cmd.cmd == VGM_CMD_PCM_SEEK) {
            // PCM seek - not directly supported in S98
            fprintf(stderr, "Skipping PCM seek to offset 0x%X\n", cmd.pcmOffset);
        } else if (cmd.cmd != VGM_CMD_END && cmd.waitSamples == 0 && 
                   cmd.cmd != VGM_CMD_DATA_BLOCK && cmd.cmd != VGM_CMD_PCM_SEEK) {
            // Unknown command
            unknownCount++;
            if (unknownCount <= 10) {
                fprintf(stderr, "Debug: Unhandled command 0x%02X (reg=%u, data=%u)\n", 
                        cmd.cmd, cmd.reg, cmd.data);
            }
        }
    }
    
    fprintf(stderr, "Conversion complete. Total samples: %u\n", totalSamples);
    fprintf(stderr, "Register writes: %u, Wait commands: %u\n", 
            regWriteCount, waitCount);
    
    // Build tag map: start with GD3 metadata from the VGM
    std::map<std::string, std::string> tags;
    ExtractGD3Tags(inputFile, tags);

    // S98 has no native gain field, so store the VGM volume modifier as a tag.
    // Volume = 2 ^ (volumeModifier / 32.0); default 0 => factor 1.0.
    if (vgmHeader.volumeModifier != 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", (int)vgmHeader.volumeModifier);
        tags["vgm_volume_modifier"] = buf;
        fprintf(stderr, "Volume modifier tag written: vgm_volume_modifier=%d\n",
                (int)vgmHeader.volumeModifier);
    }

    if (!tags.empty()) {
        writer.WriteTag(tags);
        fprintf(stderr, "Tags written\n");
    }
    
    // Finalize S98 file
    writer.Finalize();
    writer.Close();
    reader.Close();
    
    fprintf(stderr, "S98 file written: %s\n", outputFile);
    return 0;
}

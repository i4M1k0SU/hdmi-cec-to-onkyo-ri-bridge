#pragma once
#include <stdint.h>

// CEC logical addresses
#define CEC_ADDR_AUDIO_SYSTEM  0x05
#define CEC_ADDR_BROADCAST     0x0F

// CEC opcodes
#define CEC_OP_FEATURE_ABORT              0x00
#define CEC_OP_IMAGE_VIEW_ON              0x04
#define CEC_OP_TEXT_VIEW_ON               0x0D
#define CEC_OP_STANDBY                    0x36
#define CEC_OP_USER_CONTROL_PRESSED       0x44
#define CEC_OP_USER_CONTROL_RELEASED      0x45
#define CEC_OP_GIVE_OSD_NAME              0x46
#define CEC_OP_SET_OSD_NAME               0x47
#define CEC_OP_SYSTEM_AUDIO_MODE_REQUEST  0x70
#define CEC_OP_GIVE_AUDIO_STATUS          0x71
#define CEC_OP_SET_SYSTEM_AUDIO_MODE      0x72
#define CEC_OP_SET_AUDIO_VOLUME_LEVEL     0x73
#define CEC_OP_REPORT_AUDIO_STATUS        0x7A
#define CEC_OP_GIVE_SYSTEM_AUDIO_MODE_STATUS 0x7D
#define CEC_OP_SYSTEM_AUDIO_MODE_STATUS   0x7E
#define CEC_OP_ACTIVE_SOURCE              0x82
#define CEC_OP_GIVE_PHYSICAL_ADDRESS      0x83
#define CEC_OP_REPORT_PHYSICAL_ADDRESS    0x84
#define CEC_OP_REQUEST_ACTIVE_SOURCE      0x85
#define CEC_OP_SET_STREAM_PATH            0x86
#define CEC_OP_DEVICE_VENDOR_ID           0x87
#define CEC_OP_GIVE_DEVICE_VENDOR_ID      0x8C
#define CEC_OP_GIVE_DEVICE_POWER_STATUS   0x8F
#define CEC_OP_REPORT_POWER_STATUS        0x90
#define CEC_OP_CEC_VERSION                0x9E
#define CEC_OP_GET_CEC_VERSION            0x9F
#define CEC_OP_INITIATE_ARC               0xC0
#define CEC_OP_REPORT_ARC_INITIATED       0xC1
#define CEC_OP_REPORT_ARC_TERMINATED      0xC2
#define CEC_OP_REQUEST_ARC_INITIATION     0xC3
#define CEC_OP_REQUEST_ARC_TERMINATION    0xC4
#define CEC_OP_TERMINATE_ARC              0xC5
#define CEC_OP_ABORT                      0xFF

// Feature Abort reasons
#define CEC_ABORT_UNRECOGNIZED   0x00
#define CEC_ABORT_WRONG_MODE     0x01
#define CEC_ABORT_CANNOT_PROVIDE 0x02
#define CEC_ABORT_INVALID        0x03
#define CEC_ABORT_REFUSED        0x04

// CEC opcode → 名前文字列
static inline const char *cec_opcode_name(uint8_t op) {
    switch (op) {
    case CEC_OP_FEATURE_ABORT:              return "Feature Abort";
    case CEC_OP_IMAGE_VIEW_ON:              return "Image View On";
    case CEC_OP_TEXT_VIEW_ON:               return "Text View On";
    case CEC_OP_STANDBY:                    return "Standby";
    case CEC_OP_USER_CONTROL_PRESSED:       return "User Control Pressed";
    case CEC_OP_USER_CONTROL_RELEASED:      return "User Control Released";
    case CEC_OP_GIVE_OSD_NAME:              return "Give OSD Name";
    case CEC_OP_SET_OSD_NAME:               return "Set OSD Name";
    case CEC_OP_SYSTEM_AUDIO_MODE_REQUEST:  return "System Audio Mode Request";
    case CEC_OP_GIVE_AUDIO_STATUS:          return "Give Audio Status";
    case CEC_OP_SET_SYSTEM_AUDIO_MODE:      return "Set System Audio Mode";
    case CEC_OP_SET_AUDIO_VOLUME_LEVEL:     return "Set Audio Volume Level";
    case CEC_OP_REPORT_AUDIO_STATUS:        return "Report Audio Status";
    case CEC_OP_GIVE_SYSTEM_AUDIO_MODE_STATUS: return "Give System Audio Mode Status";
    case CEC_OP_SYSTEM_AUDIO_MODE_STATUS:   return "System Audio Mode Status";
    case CEC_OP_ACTIVE_SOURCE:              return "Active Source";
    case CEC_OP_GIVE_PHYSICAL_ADDRESS:      return "Give Physical Address";
    case CEC_OP_REPORT_PHYSICAL_ADDRESS:    return "Report Physical Address";
    case CEC_OP_REQUEST_ACTIVE_SOURCE:      return "Request Active Source";
    case CEC_OP_SET_STREAM_PATH:            return "Set Stream Path";
    case CEC_OP_DEVICE_VENDOR_ID:           return "Device Vendor ID";
    case CEC_OP_GIVE_DEVICE_VENDOR_ID:      return "Give Device Vendor ID";
    case CEC_OP_GIVE_DEVICE_POWER_STATUS:   return "Give Device Power Status";
    case CEC_OP_REPORT_POWER_STATUS:        return "Report Power Status";
    case CEC_OP_CEC_VERSION:                return "CEC Version";
    case CEC_OP_GET_CEC_VERSION:            return "Get CEC Version";
    case CEC_OP_INITIATE_ARC:               return "Initiate ARC";
    case CEC_OP_REPORT_ARC_INITIATED:       return "Report ARC Initiated";
    case CEC_OP_REPORT_ARC_TERMINATED:      return "Report ARC Terminated";
    case CEC_OP_REQUEST_ARC_INITIATION:     return "Request ARC Initiation";
    case CEC_OP_REQUEST_ARC_TERMINATION:    return "Request ARC Termination";
    case CEC_OP_TERMINATE_ARC:              return "Terminate ARC";
    case CEC_OP_ABORT:                      return "Abort";
    default:                                return "Unknown";
    }
}

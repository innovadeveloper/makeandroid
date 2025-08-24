#ifndef COMMAND_H
#define COMMAND_H

enum class Command : int {
    // Security level commands
    AUTHENTICATE_DES_2K3DES = 0x0A,
    AUTHENTICATE_3K3DES = 0x1A,
    AUTHENTICATE_AES = 0xAA,
    CHANGE_KEY_SETTINGS = 0x54,
    SET_CONFIGURATION = 0x5C,
    CHANGE_KEY = 0xC4,
    GET_KEY_VERSION = 0x64,

    // PICC level commands
    CREATE_APPLICATION = 0xCA,
    DELETE_APPLICATION = 0xDA,
    GET_APPLICATIONS_IDS = 0x6A,
    FREE_MEMORY = 0x6E,
    GET_DF_NAMES = 0x6D,
    GET_KEY_SETTINGS = 0x45,
    SELECT_APPLICATION = 0x5A,
    FORMAT_PICC = 0xFC,
    GET_VERSION = 0x60,
    GET_CARD_UID = 0x51,

    // Application level commands
    GET_FILE_IDS = 0x6F,
    GET_FILE_SETTINGS = 0xF5,
    CHANGE_FILE_SETTINGS = 0x5F,
    CREATE_STD_DATA_FILE = 0xCD,
    CREATE_BACKUP_DATA_FILE = 0xCB,
    CREATE_VALUE_FILE = 0xCC,
    CREATE_LINEAR_RECORD_FILE = 0xC1,
    CREATE_CYCLIC_RECORD_FILE = 0xC0,
    DELETE_FILE = 0xDF,

    // File level commands
    READ_DATA = 0xBD,
    WRITE_DATA = 0x3D,
    GET_VALUE = 0x6C,
    CREDIT = 0x0C,
    DEBIT = 0xDC,
    LIMITED_CREDIT = 0x1C,
    WRITE_RECORDS = 0x3B,
    READ_RECORDS = 0xBB,
    CLEAR_RECORD_FILE = 0xEB,
    COMMIT_TRANSACTION = 0xC7,
    ABORT_TRANSACTION = 0xA7,

    // Protocol commands
    MORE = 0xAF,
    UNKNOWN_COMMAND = 1001
};

class CommandHelper {
public:
    static int getCode(Command cmd);
    static Command getCommand(int code);
};

#endif // COMMAND_H
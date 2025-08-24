#include <jni.h>
#include <string>
#include "command.h"

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getIntegerValue(JNIEnv *env, jobject thiz) {
    return 42;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_innova_native_NativeLibraryExecutor_getBooleanValue(JNIEnv *env, jobject thiz) {
    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_innova_native_NativeLibraryExecutor_getStringValue(JNIEnv *env, jobject thiz) {
    std::string message = "Hello from Native Library";
    return env->NewStringUTF(message.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_addNumbers(JNIEnv *env, jobject thiz, jint a, jint b) {
    return a + b;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_innova_native_NativeLibraryExecutor_isEven(JNIEnv *env, jobject thiz, jint number) {
    return (number % 2 == 0) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_innova_native_NativeLibraryExecutor_formatString(JNIEnv *env, jobject thiz, jstring input) {
    const char *inputStr = env->GetStringUTFChars(input, nullptr);
    std::string formatted = "Formatted: " + std::string(inputStr);
    env->ReleaseStringUTFChars(input, inputStr);
    return env->NewStringUTF(formatted.c_str());
}

// Command enum helper functions implementation
int CommandHelper::getCode(Command cmd) {
    return static_cast<int>(cmd);
}

Command CommandHelper::getCommand(int code) {
    switch (code) {
        case 0x0A: return Command::AUTHENTICATE_DES_2K3DES;
        case 0x1A: return Command::AUTHENTICATE_3K3DES;
        case 0xAA: return Command::AUTHENTICATE_AES;
        case 0x54: return Command::CHANGE_KEY_SETTINGS;
        case 0x5C: return Command::SET_CONFIGURATION;
        case 0xC4: return Command::CHANGE_KEY;
        case 0x64: return Command::GET_KEY_VERSION;
        case 0xCA: return Command::CREATE_APPLICATION;
        case 0xDA: return Command::DELETE_APPLICATION;
        case 0x6A: return Command::GET_APPLICATIONS_IDS;
        case 0x6E: return Command::FREE_MEMORY;
        case 0x6D: return Command::GET_DF_NAMES;
        case 0x45: return Command::GET_KEY_SETTINGS;
        case 0x5A: return Command::SELECT_APPLICATION;
        case 0xFC: return Command::FORMAT_PICC;
        case 0x60: return Command::GET_VERSION;
        case 0x51: return Command::GET_CARD_UID;
        case 0x6F: return Command::GET_FILE_IDS;
        case 0xF5: return Command::GET_FILE_SETTINGS;
        case 0x5F: return Command::CHANGE_FILE_SETTINGS;
        case 0xCD: return Command::CREATE_STD_DATA_FILE;
        case 0xCB: return Command::CREATE_BACKUP_DATA_FILE;
        case 0xCC: return Command::CREATE_VALUE_FILE;
        case 0xC1: return Command::CREATE_LINEAR_RECORD_FILE;
        case 0xC0: return Command::CREATE_CYCLIC_RECORD_FILE;
        case 0xDF: return Command::DELETE_FILE;
        case 0xBD: return Command::READ_DATA;
        case 0x3D: return Command::WRITE_DATA;
        case 0x6C: return Command::GET_VALUE;
        case 0x0C: return Command::CREDIT;
        case 0xDC: return Command::DEBIT;
        case 0x1C: return Command::LIMITED_CREDIT;
        case 0x3B: return Command::WRITE_RECORDS;
        case 0xBB: return Command::READ_RECORDS;
        case 0xEB: return Command::CLEAR_RECORD_FILE;
        case 0xC7: return Command::COMMIT_TRANSACTION;
        case 0xA7: return Command::ABORT_TRANSACTION;
        case 0xAF: return Command::MORE;
        default: return Command::UNKNOWN_COMMAND;
    }
}

// JNI functions for Command enum
extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getCommandCode(JNIEnv *env, jobject thiz, jint commandOrdinal) {
    Command cmd = static_cast<Command>(commandOrdinal);
    return CommandHelper::getCode(cmd);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getCommandFromCode(JNIEnv *env, jobject thiz, jint code) {
    Command cmd = CommandHelper::getCommand(code);
    return static_cast<jint>(cmd);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getAuthenticateDes2K3DesCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::AUTHENTICATE_DES_2K3DES);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getAuthenticate3K3DesCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::AUTHENTICATE_3K3DES);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getAuthenticateAesCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::AUTHENTICATE_AES);
}

// All Command enum values as JNI functions
extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getChangeKeySettingsCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::CHANGE_KEY_SETTINGS);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getSetConfigurationCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::SET_CONFIGURATION);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getChangeKeyCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::CHANGE_KEY);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getGetKeyVersionCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::GET_KEY_VERSION);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getCreateApplicationCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::CREATE_APPLICATION);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getDeleteApplicationCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::DELETE_APPLICATION);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getGetApplicationsIdsCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::GET_APPLICATIONS_IDS);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getFreeMemoryCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::FREE_MEMORY);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getGetDfNamesCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::GET_DF_NAMES);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getGetKeySettingsCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::GET_KEY_SETTINGS);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getSelectApplicationCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::SELECT_APPLICATION);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getFormatPiccCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::FORMAT_PICC);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getGetVersionCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::GET_VERSION);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getGetCardUidCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::GET_CARD_UID);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getGetFileIdsCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::GET_FILE_IDS);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getGetFileSettingsCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::GET_FILE_SETTINGS);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getChangeFileSettingsCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::CHANGE_FILE_SETTINGS);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getCreateStdDataFileCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::CREATE_STD_DATA_FILE);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getCreateBackupDataFileCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::CREATE_BACKUP_DATA_FILE);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getCreateValueFileCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::CREATE_VALUE_FILE);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getCreateLinearRecordFileCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::CREATE_LINEAR_RECORD_FILE);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getCreateCyclicRecordFileCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::CREATE_CYCLIC_RECORD_FILE);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getDeleteFileCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::DELETE_FILE);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getReadDataCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::READ_DATA);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getWriteDataCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::WRITE_DATA);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getGetValueCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::GET_VALUE);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getCreditCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::CREDIT);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getDebitCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::DEBIT);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getLimitedCreditCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::LIMITED_CREDIT);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getWriteRecordsCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::WRITE_RECORDS);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getReadRecordsCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::READ_RECORDS);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getClearRecordFileCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::CLEAR_RECORD_FILE);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getCommitTransactionCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::COMMIT_TRANSACTION);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getAbortTransactionCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::ABORT_TRANSACTION);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getMoreCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::MORE);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_innova_native_NativeLibraryExecutor_getUnknownCommandCode(JNIEnv *env, jobject thiz) {
    return CommandHelper::getCode(Command::UNKNOWN_COMMAND);
}
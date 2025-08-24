package com.innova.native

public class NativeLibraryExecutor {

    companion object {
        init {
            System.loadLibrary("nativelibrary")
        }
    }

    external fun getIntegerValue(): Int

    external fun getBooleanValue(): Boolean

    external fun getStringValue(): String

    external fun addNumbers(a: Int, b: Int): Int

    external fun isEven(number: Int): Boolean

    external fun formatString(input: String): String

    // All Command enum values as external functions
    external fun getAuthenticateDes2K3DesCode(): Int
    external fun getAuthenticate3K3DesCode(): Int
    external fun getAuthenticateAesCode(): Int
    external fun getChangeKeySettingsCode(): Int
    external fun getSetConfigurationCode(): Int
    external fun getChangeKeyCode(): Int
    external fun getGetKeyVersionCode(): Int
    external fun getCreateApplicationCode(): Int
    external fun getDeleteApplicationCode(): Int
    external fun getGetApplicationsIdsCode(): Int
    external fun getFreeMemoryCode(): Int
    external fun getGetDfNamesCode(): Int
    external fun getGetKeySettingsCode(): Int
    external fun getSelectApplicationCode(): Int
    external fun getFormatPiccCode(): Int
    external fun getGetVersionCode(): Int
    external fun getGetCardUidCode(): Int
    external fun getGetFileIdsCode(): Int
    external fun getGetFileSettingsCode(): Int
    external fun getChangeFileSettingsCode(): Int
    external fun getCreateStdDataFileCode(): Int
    external fun getCreateBackupDataFileCode(): Int
    external fun getCreateValueFileCode(): Int
    external fun getCreateLinearRecordFileCode(): Int
    external fun getCreateCyclicRecordFileCode(): Int
    external fun getDeleteFileCode(): Int
    external fun getReadDataCode(): Int
    external fun getWriteDataCode(): Int
    external fun getGetValueCode(): Int
    external fun getCreditCode(): Int
    external fun getDebitCode(): Int
    external fun getLimitedCreditCode(): Int
    external fun getWriteRecordsCode(): Int
    external fun getReadRecordsCode(): Int
    external fun getClearRecordFileCode(): Int
    external fun getCommitTransactionCode(): Int
    external fun getAbortTransactionCode(): Int
    external fun getMoreCode(): Int
    external fun getUnknownCommandCode(): Int
}
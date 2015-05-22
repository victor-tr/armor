#include "pult_requests.h"

#include "common/debug_macro.h"
#include "common/errors.h"

#include "pult/pult_tx_buffer.h"

#include "db/fs.h"

#include "service/crc.h"

#include "states/armingstates.h"

#include "core/system.h"

s32 rx_handler_requestReject(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_requestReject");


    return RETURN_NO_ERRORS;
}


s32 rx_handler_deviceInfo(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_deviceInfo\r\n");

    SystemInfo *pSysInfo = getSystemInfo(NULL);

    u8 data[5] = {1,
                  pSysInfo->firmware_version_major,
                  pSysInfo->firmware_version_minor,
                  0,
                  1};
    return reportToPultComplex(data, sizeof(data), PMC_Response_DeviceInfo, PMQ_AuxInfoMsg);
}


s32 rx_handler_deviceState(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_deviceState\r\n");


    return RETURN_NO_ERRORS;
}


s32 rx_handler_loopsState(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_loopsState\r\n");
    Zone_tbl *pZones = getZoneTable(NULL);
    Zone *pZone = 0;
    ArmingGroup *pGroup = 0;
    u16 dataLen;

    u16 firstZonePKT = pMsg->complex_msg_part[0];
    u16 zoneCountPKT = pMsg->complex_msg_part[1];
    u16 detailLevel =  pMsg->complex_msg_part[2];


    // zones count
    if(firstZonePKT == 0)
        zoneCountPKT == pZones->size;
    else
        if((firstZonePKT + zoneCountPKT) > pZones->size )
            zoneCountPKT = pZones->size - firstZonePKT;

    // buffer size
    switch (detailLevel){
    case 3:
        dataLen = zoneCountPKT * 2;
        break;
    case 2:
        dataLen = zoneCountPKT / 2;
        if(zoneCountPKT % 2) dataLen++;
        break;
    case 1:
    default:
        dataLen = zoneCountPKT / 8;
        if(zoneCountPKT % 8) dataLen++;
        break;
    }

    // create buffer
    u8 *data = Ql_MEM_Alloc(dataLen + 3);
    if(!data)
    {
        OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", dataLen + 3);
        u8 data_err[2] = {RCE_MemoryAllocationError,0};
        reportToPultComplex(data_err, 2, PMC_CommandReject, PMQ_AuxInfoMsg);
        return ERR_GET_NEW_MEMORY_FAILED;
    }

    u16 indexData = 0;
    u8 tempData;
    u8 zoneState;
    u8 zoneEnabled;
    u8 zoneArmed;
    u16 zoneGroupID;
    u8 zoneType;
    u8 zoneLastEventType;
    u8 counterBit = 0; // from 0 to 7
    u8 zoneStateBuffer = 0;
    u8 zoneTetradaBuffer = 0;

    data[0] = firstZonePKT;
    data[1] = zoneCountPKT;
    data[2] = detailLevel;

    for (u16 i = firstZonePKT; i < zoneCountPKT; ++i) {
        pZone = &pZones->t[i];

        tempData = 0;

        zoneState =(pZone->zoneDamage_counter == 0)? 1 : 0;
        zoneEnabled = (pZone->enabled == TRUE)? 1 : 0;
        pGroup = getArmingGroupByID(pZone->group_id);
        if(!pGroup)
            zoneArmed = 0;
        else
            zoneArmed = (TRUE == Ar_GroupState_isArmed(pGroup))? 1 : 0;
        zoneGroupID = pZone->group_id;
        zoneType = pZone->zone_type;
        zoneLastEventType = pZone->last_event_type;

        switch (detailLevel){
        case 3:
            indexData = (i - firstZonePKT) * 2 + 3;
            break;
        case 2:
            indexData = (i - firstZonePKT) / 2 + 3;

            zoneTetradaBuffer = 0;
            zoneTetradaBuffer = (zoneTetradaBuffer | zoneEnabled) << 1;
            zoneTetradaBuffer = (zoneTetradaBuffer | zoneArmed) << 1;
            if((i - firstZonePKT) % 2)
                tempData = tempData << 4;



            break;
        case 1:
        default:
            //
            indexData = (i - firstZonePKT) / 8 + 3;

            zoneStateBuffer = (zoneStateBuffer | zoneState) << 1;
            counterBit++;
            if(counterBit == 8 || (i ==(zoneCountPKT -1)))
            {
                counterBit = 0;
                data[indexData] = zoneStateBuffer;
                zoneStateBuffer = 0;
            }


            break;
        }



    }

    reportToPultComplex(data, sizeof(data), PMC_Response_LoopsState, PMQ_AuxInfoMsg);
    Ql_MEM_Free(data);
    return RETURN_NO_ERRORS;
}


s32 rx_handler_groupsState(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_groupsState\r\n");

    ArmingGroup_tbl *pGroups = getArmingGroupTable(NULL);
    ArmingGroup *pGroup = 0;

    u16 dataLen;
    u16 firstGroupPKT = pMsg->complex_msg_part[0];
    u16 groupCountPKT = pMsg->complex_msg_part[1];

    // group count
    if(firstGroupPKT == 0)
        groupCountPKT == pGroups->size;
    else
        if((firstGroupPKT + groupCountPKT) > pGroups->size )
            groupCountPKT = pGroups->size - firstGroupPKT;

    // buffer size
    dataLen = groupCountPKT / 2;
    if(groupCountPKT % 2) dataLen++;

    // create buffer
    u8 *data = Ql_MEM_Alloc(dataLen + 2);
    if(!data)
    {
        OUT_DEBUG_1("Failed to get %d bytes from HEAP\r\n", dataLen + 2);
        u8 data_err[2] = {RCE_MemoryAllocationError,0};
        reportToPultComplex(data_err, 2, PMC_CommandReject, PMQ_AuxInfoMsg);
        return ERR_GET_NEW_MEMORY_FAILED;
    }

    u16 indexData = 0;
    u8 tempData;

    data[0] = firstGroupPKT;
    data[1] = groupCountPKT;
        for (u16 i = firstGroupPKT; i < groupCountPKT; ++i) {
            pGroup = &pGroups->t[i];
            indexData = (i - firstGroupPKT) / 2 + 2;

            // --- Set  bits
            // bit0: 1-Normal/0-Others
            // bit1: reserved
            // bit2: 1-Armrd /0-Others
            // bit3: 1-Used  /0-Others
            tempData = 0;
            if(TRUE == Ar_GroupState_isArmed(pGroup))
                tempData  = tempData | 4;

            if(TRUE == Ar_System_isZonesInGroupNormal(pGroup))
                tempData  = tempData | 1;
            // ---

            if((i - firstGroupPKT) % 2)
                tempData = tempData << 4;

            data[indexData] = data[indexData] | tempData;
        }

    reportToPultComplex(data, sizeof(data), PMC_Response_GroupsState, PMQ_AuxInfoMsg);
    Ql_MEM_Free(data);
    return RETURN_NO_ERRORS;


}


s32 rx_handler_readTable(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_readTable\r\n");


    return RETURN_NO_ERRORS;
}

s32 rx_handler_saveTableSegment(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_saveTableSegment\r\n");



    RxConfiguratorPkt pkt;

    pkt.dataType=pMsg->complex_msg_part[0];
    pkt.data=&pMsg->complex_msg_part[4];


    pkt.datalen= pMsg->part_len-4;

    pkt.marker=CONFIGURATION_AUTOSYNC_PACKET_MARKER;
    pkt.pktIdx=0;
    pkt.repeatCounter=0;
    pkt.sessionKey=0;
    pkt.bAppend=TRUE;


    char name[30] = {0};

    switch (pkt.dataType) {
    case OBJ_PHONELIST_SIM1:
        Ql_strcpy(name, FILENAME_PATTERN_PHONELIST_SIM1);
        break;
    case OBJ_PHONELIST_SIM2:
        Ql_strcpy(name, FILENAME_PATTERN_PHONELIST_SIM2);
        break;
    case OBJ_IPLIST_SIM1:
        Ql_strcpy(name, FILENAME_PATTERN_IPLIST_SIM1);
        break;
    case OBJ_IPLIST_SIM2:
        Ql_strcpy(name, FILENAME_PATTERN_IPLIST_SIM2);
        break;
    default:
        break;
    }

    //===================================
    // PREPARE Empty file
    if(0 == pMsg->complex_msg_part[1])
    {
        s32 ret = prepareEmptyDbFileInAutosyncTemporaryDB(name);
        if(ret < RETURN_NO_ERRORS)
        {
            OUT_DEBUG_1("prepareEmptyDbFileInAutosyncTemporaryDB() = %d error. File not create.\r\n", ret);
            return ret;
        }
    }

    //===================================
    // SAVE to AutosyncTemporaryDB
    s32 ret = saveSettingsToAutosyncTemporaryDB(&pkt);
    if(ret < RETURN_NO_ERRORS)
    {
        OUT_DEBUG_1("saveSettingsToAutosyncTemporaryDB() = %d error\r\n", ret);
        return ret;
    }


    //===================================
    // MOVE to work file
    if(pMsg->complex_msg_part[1] == pMsg->complex_msg_part[2])
    {
        if(!copyFilleFromAutosyncTemporaryDBToWorkDB(name))
        {
            OUT_DEBUG_1("copyFilleFromAutosyncTemporaryDBToWorkDB() = %d error. File not move.\r\n");
            return ERR_DB_SAVING_FAILED;
        }

        // ACTUALIZATION
        ret = actualizationSIMCardLists(pkt.dataType);
        if(ret < RETURN_NO_ERRORS)
        {
            OUT_DEBUG_1("actualizationSIMCardLists() = %d error. Actualize error.\r\n", ret);
            return ret;
        }

        SystemInfo *pSysInfo = getSystemInfo(NULL);
        pSysInfo->settings_signature_autosync = pMsg->complex_msg_part[3];

        // save to DB signature autosync
        ret = saveToDbFile(DBFILE(FILENAME_PATTERN_SYSTEMINFO), pSysInfo, sizeof(SystemInfo), FALSE);
        if(ret < RETURN_NO_ERRORS)
        {
            OUT_DEBUG_1("saveToDbFile() = %d error. Save signature autosynce error.\r\n", ret);
            return ret;
        }
        OUT_DEBUG_7("SystemInfo settings_signature_autosync = %d.\r\n", pSysInfo->settings_signature_autosync);
    }

    return RETURN_NO_ERRORS;
}


s32 rx_handler_requestVoiceCall(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_requestVoiceCall\r\n");


    return RETURN_NO_ERRORS;
}


s32 rx_handler_settingsData(PultMessageRx *pMsg)
{
    OUT_DEBUG_2("rx_handler_settingsData\r\n");



    u8 *buf = pMsg->complex_msg_part;
    if(!checkConfiguratorCRC(buf))
    {
        OUT_DEBUG_1("rx_handler_settingsData() = CRC settingsPKT error.\r\n");
        return ERR_BAD_CRC;
    }



    RxConfiguratorPkt pkt;

    pkt.bAppend =       buf[SPS_APPEND_FLAG];
    pkt.data =         &buf[SPS_PREFIX];
    pkt.datalen =       buf[SPS_BYTES_QTY_H] << 8 | buf[SPS_BYTES_QTY_L];
    pkt.dataType =      (DbObjectCode)buf[SPS_FILECODE];
    pkt.marker =        buf[SPS_MARKER];
    pkt.pktIdx =        buf[SPS_PKT_INDEX];
    pkt.repeatCounter = buf[SPS_PKT_REPEAT];
    pkt.sessionKey =    buf[SPS_PKT_KEY];

    s32 ret = Ar_Configurator_processSettingsData(&pkt, CCM_RemotePultConnection);
    if (ret < RETURN_NO_ERRORS) {
        OUT_DEBUG_1("Ar_Configurator_processSettingsData() = %d error.\r\n", ret);
    }


    return RETURN_NO_ERRORS;
}

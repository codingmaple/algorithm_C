#include <iostream>
#include <windef.h>
#include <afxres.h>
#include <unistd.h>
#include "libmodbus/modbus.h"
#include "libmodbus/unit-test.h"
#include "inc/json.hpp"
#include "inc/json_fwd.hpp"
#include "string"
#include <ctime>

using json = nlohmann::json;
using namespace std;

/* For MinGW */
#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif

#define BUG_REPORT(_cond, _format, _args ...) \
    printf("\nLine %d: assertion error for '%s': " _format "\n", __LINE__, # _cond, ## _args)


typedef int (WINAPI *PROC1)(long,long,long);


DWORD WINAPI mytimerproc1(LPVOID lvParamter);

DWORD WINAPI mytimerproc2(LPVOID lvParamter);

DWORD WINAPI mytimerproc3(LPVOID lvParamter);


void CALLBACK threadModbusInput (HWND hwnd, UINT message, UINT idTimer, DWORD dwTime);

void CALLBACK threadJs (HWND hwnd, UINT message, UINT idTimer, DWORD dwTime);

void CALLBACK threadHw (HWND hwnd, UINT message, UINT idTimer, DWORD dwTime);

bool hwStart = false;
bool settings_en = false;
bool statusBit_en = false;
json settings_json;
int hwData[20];
int mV = 0;


modbus_t *ctx = NULL;
uint32_t old_response_to_sec;
uint32_t old_response_to_usec;
uint32_t new_response_to_sec;
uint32_t new_response_to_usec;


int rc;
uint8_t *tab_rp_bits = NULL;
uint16_t *tab_rp_registers = NULL;
int nb_points = 52;

PROC1 SendDataToHW;
HINSTANCE hinstLib;


int main() {

    ctx = modbus_new_rtu("COM4", 115200, 'N', 8, 1);

    if (ctx == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
//        return -1;
    }
    modbus_set_debug(ctx, FALSE);
    modbus_set_error_recovery(ctx,
                              (modbus_error_recovery_mode) (MODBUS_ERROR_RECOVERY_LINK |
                                                            MODBUS_ERROR_RECOVERY_PROTOCOL));

    modbus_set_slave(ctx, 1);


    modbus_get_response_timeout(ctx, &old_response_to_sec, &old_response_to_usec);

    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
    }
    modbus_get_response_timeout(ctx, &new_response_to_sec, &new_response_to_usec);


    hinstLib = LoadLibrary(TEXT("C:\\Windows\\SysWOW64\\HWSendData.dll"));

    if (hinstLib != NULL) {
        SendDataToHW = (PROC1) GetProcAddress(hinstLib, "SendDataToHW");
    }

    try{

        HANDLE handle1 = (HANDLE)::CreateThread(NULL, 0, mytimerproc1, NULL, 0, NULL);

        HANDLE handle2 = (HANDLE)::CreateThread(NULL, 0, mytimerproc2, NULL, 0, NULL);

        HANDLE handle3 = (HANDLE)::CreateThread(NULL, 0, mytimerproc3, NULL, 0, NULL);

        HANDLE hArray[3] = { handle1 ,handle2, handle3 };
        WaitForMultipleObjects(2, hArray, TRUE, INFINITE);

        CloseHandle(handle1);
        CloseHandle(handle2);
        CloseHandle(handle3);
        if (hinstLib != NULL){
            FreeLibrary(hinstLib);
        }
    } catch (...) {
        printf("MonitorTeachingSoftPlayer error...\n");
    }
	return 0;
}

DWORD  WINAPI mytimerproc1(LPVOID args){
    BOOL bRet = FALSE;
    MSG msg = { 0 };
    UINT timerId3 = SetTimer(NULL, 0, 200, (TIMERPROC) threadHw);
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0 ){
        if (bRet == -1){

        }else{
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    KillTimer(NULL, timerId3);
    return 0;
}

DWORD  WINAPI mytimerproc2(LPVOID args){
    BOOL bRet = FALSE;
    MSG msg = { 0 };
    UINT timerId3 = SetTimer(NULL, 0, 1, (TIMERPROC) threadJs);
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0 ){
        if (bRet == -1){

        }else{
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    KillTimer(NULL, timerId3);
    return 0;
}

DWORD  WINAPI mytimerproc3(LPVOID args){
    BOOL bRet = FALSE;
    MSG msg = { 0 };

    UINT timerId3 = SetTimer(NULL, 0, 200, (TIMERPROC) threadModbusInput);
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0 ){
        if (bRet == -1){

        }else{
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    KillTimer(NULL, timerId3);
    return 0;
}



void CALLBACK threadModbusInput (HWND hwnd, UINT message, UINT idTimer, DWORD dwTime) {

    tab_rp_bits = (uint8_t *) malloc(nb_points * sizeof(uint8_t));
    memset(tab_rp_bits, 0, nb_points * sizeof(uint8_t));
    tab_rp_registers = (uint16_t *) malloc(nb_points * sizeof(uint16_t));
    memset(tab_rp_registers, 0, nb_points * sizeof(uint16_t));

    json modbus_reg_json[52];
    rc = modbus_read_registers(ctx, 0, 52, tab_rp_registers);
    for (int i = 0; i < 52; i++) {
        modbus_reg_json[i]["addr"] = i;
        modbus_reg_json[i]["value"] = (int) (tab_rp_registers[i]);
    }

    json modbus_json;

    modbus_json["modbus_data"] = modbus_reg_json;

    json read_coils[5];
    rc = modbus_read_input_bits(ctx, 0, 5, tab_rp_bits);
    for (int i = 0; i < 5; i++) {
        read_coils[i]["addr"] = i;
        read_coils[i]["value"] = (int) (tab_rp_bits[i]);
    }

    modbus_json["read_coils"] = read_coils;

    json mv_json[20];
    rc = modbus_read_registers(ctx, 400, 40, tab_rp_registers);
    int count = 0;
    for (int i = 0; i < 40; i += 2, ++count) {
        mv_json[count]["addr"] = count + 400;
        int value = 0;
        short high = (short) (tab_rp_registers[i]);
        short low = (short) (tab_rp_registers[i + 1]);
        value |= (high & 0x0000ffff);
        value = (value << 16) | (low & 0x0000ffff);
        mv_json[count]["value"] = value;
    }
    modbus_json["modbus_mv"] = mv_json;
    for (int i = 0; i < 20; ++i) {
        hwData[i] = (int) (tab_rp_registers[i]);
    }
    cout << modbus_json << endl;
}


void CALLBACK threadHw (HWND hwnd, UINT message, UINT idTimer, DWORD dwTime)
{
    if (SendDataToHW != NULL) {
        int testing = false;
        for(int i = 0; i < 1; i++){
            ((SendDataToHW)(hwData[i], 0, 3));//Send channel A data
            ((SendDataToHW)(0, 0, 4));//Send channel B data as 0
        }

        if(hwStart){
            ((SendDataToHW)(1, 0, 1));//Start channel A
            testing = true;
        }
        if((!hwStart) && testing){
            ((SendDataToHW)(1, 0, 2));//Stop channel A
            testing = false;
        }
    }
}


void CALLBACK threadJs (HWND hwnd, UINT message, UINT idTimer, DWORD dwTime){

    cin >> settings_json;
    if(settings_json.count("cmd")){
        auto cmdStr = settings_json["cmd"];
        if(cmdStr=="start_testing"){
            hwStart = true;
        } else if(cmdStr=="stop_testing"){
            hwStart = false;
        } else if(cmdStr=="power_off"){

        } else {

        }
    }
    else if(settings_json.count("settings")){
        settings_en = true;
    }
    else if(settings_json.count("status_bits")){
        statusBit_en = true;
    }

    if (settings_en) {
        auto settings = settings_json["settings"];
        for (auto setting : settings) {
            int addr = setting["addr"];
            int value = setting["value"];
            rc = modbus_write_register(ctx, addr, value);
        }
        settings_en = false;
    }

    if (statusBit_en) {
        auto statusBits = settings_json["status_bits"];
        uint8_t *bits;
        int bitCount = statusBits.size();
        bits = (uint8_t *) malloc(bitCount * sizeof(uint8_t));
        int i = 0;
        for (auto bit : statusBits) {
            int addr = bit["addr"];
            int value = bit["value"];
            rc = modbus_write_bit(ctx, addr, value);
        }
        statusBit_en = false;
    }
}
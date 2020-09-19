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

#define ASSERT_TRUE(_cond, _format, __args...) {  \
    if (_cond) {                                  \
        printf("OK\n");                           \
    } else {                                      \
        BUG_REPORT(_cond, _format, ## __args);    \
        goto close;                               \
    }                                             \
};

typedef int (WINAPI *PROC1)(long,long,long);

int modbus_master();

int modbus_slave();

int hw();

int json_test();

DWORD WINAPI threadHw(LPVOID lvParamter);

DWORD WINAPI threadJs(LPVOID lvParamter);

DWORD WINAPI threadModbusInput(LPVOID lvParamter);

DWORD WINAPI threadModbusOutput(LPVOID lvParamter);

HANDLE hMutex = NULL; //互斥量
HANDLE settingsMutex = NULL; //互斥量
HANDLE statusBitMutex = NULL; //互斥量
int totalNum = 100;

bool hwStart = false;
bool settings_en = false;
bool statusBit_en = false;
json settings_json;
int hwData[20];
int mV = 0;
int main() {

	hMutex = CreateMutex(NULL, FALSE, (LPCSTR)"hwStart"); //创建互斥量
	HANDLE hThread1 = CreateThread(NULL, 0, threadHw, NULL, 0, NULL);  //创建线程01
	HANDLE hThread2 = CreateThread(NULL, 0, threadJs, NULL, 0, NULL);  //创建线程02
	HANDLE hThread3 = CreateThread(NULL, 0, threadModbusInput, NULL, 0, NULL);  //创建线程03
	HANDLE hThread4 = CreateThread(NULL, 0, threadModbusOutput, NULL, 0, NULL);  //创建线程04
	CloseHandle(hThread1); //关闭句柄
	CloseHandle(hThread2); //关闭句柄
	CloseHandle(hThread3); //关闭句柄
	CloseHandle(hThread4); //关闭句柄
	while (true){}
	return 0;

}

DWORD WINAPI threadHw(LPVOID lvParamter)
{
	PROC1 SendDataToHW;
	HINSTANCE hinstLib = LoadLibrary(TEXT("C:\\Windows\\SysWOW64\\HWSendData.dll"));
	if (hinstLib != NULL) {
		SendDataToHW = (PROC1) GetProcAddress(hinstLib, "SendDataToHW");
		if (SendDataToHW != NULL) {
			int testing = false;
			while (TRUE){
				for(int i = 0; i < 1; i++){
					((SendDataToHW)(hwData[i], 0, 3));//Send channel A data
					((SendDataToHW)(0, 0, 4));//Send channel B data as 0
				}

				if(hwStart && (!testing)){
					((SendDataToHW)(1, 0, 1));//Start channel A
					testing = true;
				}
				if((!hwStart) && testing){
					((SendDataToHW)(1, 0, 2));//Stop channel A
					testing = false;
				}
				Sleep(200);//工作站主程序每接收到一个数据就默认地认为过了50ms。可以在HWFrequence.txt中来重新定义这种默认值。
			}
		}
		FreeLibrary(hinstLib);
	}
//	WaitForSingleObject(hMutex, INFINITE); //互斥锁
//
//	if(hwStart){
//		cout << "hw start" << endl;
//	} else {
//		//cout << "hw not start" << endl;
//	}
//
//	ReleaseMutex(hMutex); //释放互斥锁
//	Sleep(10);
}

DWORD WINAPI threadJs(LPVOID lvParamter)
{
	while (TRUE)
	{
//		int cmdStr = 0;
		cin >> settings_json;
//		if(settings_json["hw_start"].is_string()){
		if(settings_json.count("cmd")){
			auto cmdStr = settings_json["cmd"];
			if(cmdStr=="start_testing"){
				//cout << i << endl;
				WaitForSingleObject(hMutex, INFINITE); //互斥锁
				hwStart = true;
				ReleaseMutex(hMutex); //释放互斥锁
			} else if(cmdStr=="stop_testing"){
				//cout << i << endl;
				WaitForSingleObject(hMutex, INFINITE); //互斥锁
				hwStart = false;
				ReleaseMutex(hMutex); //释放互斥锁
			} else if(cmdStr=="power_off"){

			} else {

			}
		}
		else if(settings_json.count("settings")){
			WaitForSingleObject(settingsMutex, INFINITE); //互斥锁
			settings_en = true;
			ReleaseMutex(settingsMutex); //释放互斥锁
		}
		else if(settings_json.count("status_bits")){
			WaitForSingleObject(statusBitMutex, INFINITE); //互斥锁
			statusBit_en = true;
			ReleaseMutex(statusBitMutex); //释放互斥锁
		}
		Sleep(10);
	}
}

DWORD WINAPI threadModbusInput(LPVOID lvParamter)
{
	modbus_t *ctx = NULL;
	ctx = modbus_new_rtu("COM4", 115200, 'N', 8, 1);
	if (ctx == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        return -1;
	}
	modbus_set_debug(ctx, FALSE);
	modbus_set_error_recovery(ctx,
							  (modbus_error_recovery_mode)(MODBUS_ERROR_RECOVERY_LINK |
														   MODBUS_ERROR_RECOVERY_PROTOCOL));

	modbus_set_slave(ctx, 1);

	uint32_t old_response_to_sec;
	uint32_t old_response_to_usec;
	uint32_t new_response_to_sec;
	uint32_t new_response_to_usec;
	uint32_t old_byte_to_sec;
	uint32_t old_byte_to_usec;

	modbus_get_response_timeout(ctx, &old_response_to_sec, &old_response_to_usec);
	if (modbus_connect(ctx) == -1) {
		fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
		modbus_free(ctx);
//		system("pause");
		return -1;
	}
	modbus_get_response_timeout(ctx, &new_response_to_sec, &new_response_to_usec);

	int rc;
	uint8_t *tab_rp_bits = NULL;
	uint16_t *tab_rp_registers = NULL;
	int nb_points = 52;

//	nb_points = (UT_BITS_NB > UT_INPUT_BITS_NB) ? UT_BITS_NB : UT_INPUT_BITS_NB;
	tab_rp_bits = (uint8_t *) malloc(nb_points * sizeof(uint8_t));
	memset(tab_rp_bits, 0, nb_points * sizeof(uint8_t));

//	nb_points = (UT_REGISTERS_NB > UT_INPUT_REGISTERS_NB) ?
//				UT_REGISTERS_NB : UT_INPUT_REGISTERS_NB;
	tab_rp_registers = (uint16_t *) malloc(nb_points * sizeof(uint16_t));
	memset(tab_rp_registers, 0, nb_points * sizeof(uint16_t));

	/* Single register */
//	rc = modbus_write_register(ctx, 0, 0x1234);
//	printf("1/2 modbus_write_register: ");
	//ASSERT_TRUE(rc == 1, "");
	while (TRUE)
	{
		WaitForSingleObject(settingsMutex, INFINITE); //互斥锁
		if(settings_en){
			auto settings = settings_json["settings"];
			for(auto setting : settings){
//				json reg_json;
//				settings[setting] >> reg_json;
				int addr = setting["addr"];
				int value = setting["value"];
				rc = modbus_write_register(ctx, addr, value);
			}
			settings_en = false;
		}
		ReleaseMutex(settingsMutex); //释放互斥锁
		WaitForSingleObject(statusBitMutex, INFINITE); //互斥锁
		if(statusBit_en){
			auto statusBits = settings_json["status_bits"];
			uint8_t *bits;
			int bitCount = statusBits.size();
			bits = (uint8_t*)malloc(bitCount*sizeof(uint8_t));
			int i = 0;
			for(auto bit : statusBits){
//				bits[i++] = (uint8_t)bit;
//				json reg_json;
//				settings[setting] >> reg_json;
				int addr = bit["addr"];
				int value = bit["value"];
                rc = modbus_write_bit(ctx, addr, value);
			}

			statusBit_en = false;
		}
		ReleaseMutex(statusBitMutex); //释放互斥锁

		json modbus_reg_json[52];
		rc = modbus_read_registers(ctx, 0, 52, tab_rp_registers);
		for(int i = 0; i < 52 ; i++){
			modbus_reg_json[i]["addr"] = i;
			modbus_reg_json[i]["value"] = (int)(tab_rp_registers[i]);
		}

		json modbus_json;

		modbus_json["modbus_data"] = modbus_reg_json;

		json read_coils[5];
		rc = modbus_read_input_bits(ctx, 0, 5, tab_rp_bits);
		for (int i = 0; i < 5; i++){
		    read_coils[i]["addr"] = i;
		    read_coils[i]["value"] = (int)(tab_rp_bits[i]);
		}

		modbus_json["read_coils"] = read_coils;

		json mv_json[20];
		rc = modbus_read_registers(ctx, 400, 40, tab_rp_registers);
		int count = 0;
		for(int i = 0; i < 40 ; i+=2, ++count){
			mv_json[count]["addr"] = count+400;
//			mv_json[i]["value"] = (int)(tab_rp_registers[i]);
            int value = 0;
			short high = (short)(tab_rp_registers[i]);
			short low = (short)(tab_rp_registers[i+1]);
			value |= (high & 0x0000ffff);
			value = (value << 16) | (low & 0x0000ffff);
            mv_json[count]["value"] = value;
		}
		modbus_json["modbus_mv"] = mv_json;
		WaitForSingleObject(hMutex, INFINITE); //互斥锁
		for (int i = 0; i < 20; ++i) {
			hwData[i] = (int)(tab_rp_registers[i]);
		}
        DWORD t_start, t_end;
        t_start = GetTickCount();
        cout << modbus_json << endl;
        ReleaseMutex(hMutex); //释放互斥锁
        Sleep(200);
        t_end=GetTickCount();
        cout<<t_end-t_start<<endl;
	}

	close:
/* Free the memory */
	free(tab_rp_bits);
	free(tab_rp_registers);

	/* Close the connection */
	modbus_close(ctx);
	modbus_free(ctx);




}

DWORD WINAPI threadModbusOutput(LPVOID lvParamter)
{
	while (TRUE)
	{
		WaitForSingleObject(hMutex, INFINITE); //互斥锁
		ReleaseMutex(hMutex); //释放互斥锁
		Sleep(10);
	}
}





int json_test(){
	json j;
	// add a number that is stored as double (note the implicit conversion of j to an object)
	j["pi"] = 3.141;

// add a Boolean that is stored as bool
	j["happy"] = true;

// add a string that is stored as std::string
	j["name"] = "Niels";

// add another null object by passing nullptr
	j["nothing"] = nullptr;

// add an object inside the object
	j["answer"]["everything"] = 42;

// add an array that is stored as std::vector (using an initializer list)
	j["list"] = { 1, 0, 2 };

// add another object (using an initializer list of pairs)
	j["object"] = { {"currency", "USD"}, {"value", 42.99} };

	std::cout << j << std::endl;

	std::string json_str = "{\"answer\":{\"everything\":42},\"happy\":true,\"list\":[1,0,2],\"name\":\"Niels\",\"nothing\":null,\"object\":{\"currency\":\"USD\",\"value\"\n"
			":42.99},\"pi\":3.141}";
	json j2 = R"({"answer":{"everything":42},"happy":true,"list":[1,0,2],"name":"Niels","nothing":null,"object":{"currency":"USD","value"
:42.99},"pi":3.141})"_json;

	json j3 = json::parse("{\"answer\":{\"everything\":42},\"happy\":true,\"list\":[1,0,2],\"name\":\"Niels\",\"nothing\":null,\"object\":{\"currency\":\"USD\",\"value\"\n"
								  ":42.99},\"pi\":3.141}");

	json j4;

	cin >> j4;

	std::cout << j4["name"] << std::endl;



	return 0;
}
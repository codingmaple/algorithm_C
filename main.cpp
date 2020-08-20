#include <iostream>
#include <windef.h>
#include <afxres.h>
#include <unistd.h>
#include <Windows.h>
#include "libmodbus/modbus.h"
#include "libmodbus/unit-test.h"
#include "inc/json.hpp"
#include "inc/json_fwd.hpp"
#include "string"

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
	while (1){}
	return 0;

//	std::cout << "Hello, World!" << std::endl;
//	modbus_master();
//	modbus_slave();
//	hw();
//	json_test();

//	return 0;
}

DWORD WINAPI threadHw(LPVOID lvParamter)
{
	PROC1 SendDataToHW;
	HINSTANCE hinstLib = LoadLibrary("HWSendData.dll");
	if (hinstLib != NULL) {
		SendDataToHW = (PROC1) GetProcAddress(hinstLib, "SendDataToHW");
		if (SendDataToHW != NULL) {
			int testing = false;
			while (TRUE){
				for(int i = 0; i < 20; i++){
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

				Sleep(50);//工作站主程序每接收到一个数据就默认地认为过了50ms。可以在HWFrequence.txt中来重新定义这种默认值。

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
	ctx = modbus_new_rtu("COM3", 115200, 'N', 8, 1);
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
		return -1;
	}
	modbus_get_response_timeout(ctx, &new_response_to_sec, &new_response_to_usec);

	int rc;
	uint8_t *tab_rp_bits = NULL;
	uint16_t *tab_rp_registers = NULL;
	int nb_points = 40;

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
				bits[i++] = (uint8_t)bit;
//				json reg_json;
//				settings[setting] >> reg_json;
//				int addr = bit["addr"];
//				int value = bit["value"];
			}
			rc = modbus_write_bits(ctx, 0, bitCount, bits);
			statusBit_en = false;
		}
		ReleaseMutex(statusBitMutex); //释放互斥锁

		json modbus_reg_json[40];
		rc = modbus_read_registers(ctx, 0, 40, tab_rp_registers);
		for(int i = 0; i < 40 ; i++){
			modbus_reg_json[i]["addr"] = i;
			modbus_reg_json[i]["value"] = (int)(tab_rp_registers[i]);
		}

		json modbus_json;

		modbus_json["modbus_data"] = modbus_reg_json;


		json mv_json[20];
		rc = modbus_read_registers(ctx, 400, 20, tab_rp_registers);
		for(int i = 0; i < 40 ; i++){
			mv_json[i]["addr"] = i+400;
			mv_json[i]["value"] = (int)(tab_rp_registers[i]);
		}

		modbus_json["modbus_mv"] = mv_json;


		WaitForSingleObject(hMutex, INFINITE); //互斥锁
		for (int i = 0; i < 20; ++i) {
			hwData[i] = (int)(tab_rp_registers[i]);
		}


//		json hwDataJson;
//		hwDataJson["hw_data"] = hwData;
		cout << modbus_json << endl;
		ReleaseMutex(hMutex); //释放互斥锁
		Sleep(1000);
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

int modbus_master(){
	modbus_t *ctx = NULL;
	ctx = modbus_new_rtu("COM1", 115200, 'N', 8, 1);
	if (ctx == NULL) {
		fprintf(stderr, "Unable to allocate libmodbus context\n");
		return -1;
	}
	modbus_set_debug(ctx, TRUE);
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
		return -1;
	}
	modbus_get_response_timeout(ctx, &new_response_to_sec, &new_response_to_usec);

	printf("** UNIT TESTING **\n");

	int rc;
	uint8_t *tab_rp_bits = NULL;
	uint16_t *tab_rp_registers = NULL;
	int nb_points;

	nb_points = (UT_BITS_NB > UT_INPUT_BITS_NB) ? UT_BITS_NB : UT_INPUT_BITS_NB;
	tab_rp_bits = (uint8_t *) malloc(nb_points * sizeof(uint8_t));
	memset(tab_rp_bits, 0, nb_points * sizeof(uint8_t));

	nb_points = (UT_REGISTERS_NB > UT_INPUT_REGISTERS_NB) ?
				UT_REGISTERS_NB : UT_INPUT_REGISTERS_NB;
	tab_rp_registers = (uint16_t *) malloc(nb_points * sizeof(uint16_t));
	memset(tab_rp_registers, 0, nb_points * sizeof(uint16_t));

	/* Single register */
	rc = modbus_write_register(ctx, 0, 0x1234);
//	printf("1/2 modbus_write_register: ");
	//ASSERT_TRUE(rc == 1, "");

	rc = modbus_read_registers(ctx, 0,
							   1, tab_rp_registers);
	printf("2/2 modbus_read_registers: ");
	//ASSERT_TRUE(rc == 1, "FAILED (nb points %d)\n", rc);
	//ASSERT_TRUE(tab_rp_registers[0] == 0x1234, "FAILED (%0X != %0X)\n",
	//			tab_rp_registers[0], 0x1234);
	/* End of single register */

//	rc = modbus_write_bit(ctx, UT_BITS_ADDRESS, ON);
//	printf("1/2 modbus_write_bit: ");
//	//ASSERT_TRUE(rc == 1, "");
//
//	rc = modbus_read_bits(ctx, UT_BITS_ADDRESS, 1, tab_rp_bits);
//	printf("2/2 modbus_read_bits: ");
//	//ASSERT_TRUE(rc == 1, "FAILED (nb points %d)\n", rc);
//	//(tab_rp_bits[0] == ON, "FAILED (%0X != %0X)\n",
//	//			tab_rp_bits[0], ON);
close:
/* Free the memory */
	free(tab_rp_bits);
	free(tab_rp_registers);

	/* Close the connection */
	modbus_close(ctx);
	modbus_free(ctx);
	return 0;
}

int hw() {
	PROC1 SendDataToHW;
	HINSTANCE hinstLib = LoadLibrary("HWSendData.dll");
	if (hinstLib != NULL) {
		SendDataToHW = (PROC1) GetProcAddress(hinstLib, "SendDataToHW");
		if (SendDataToHW != NULL) {
			((SendDataToHW)(1, 0, 1));//Start channel A
			int data = 0;
			while (data <
				   1000)//此循环应该用另一专门的独立采样线程来实现，而不是象本例子那样放在一个简单的while循环里。采样数据应该在仪器控制程序连上仪器后就不断地发给工作站主程序，不要等到仪器进样后再发。上述“启动图谱采集”只是通知数据处理程序开始从缓存中取出采样数据进行显示和处理而已。
			{
				std::cout << data << std::endl;
				((SendDataToHW)(data, 0, 3));//Send channel A data
				((SendDataToHW)(0, 0, 4));//Send channel B data as 0
				//				data++;
				std::cin >> data;
				Sleep(5);//工作站主程序每接收到一个数据就默认地认为过了50ms。可以在HWFrequence.txt中来重新定义这种默认值。
			}
			((SendDataToHW)(1, 0, 2));//Stop channel A
			for (int i = 0; i < 10; i++) {
				((SendDataToHW)(0, 0, 3));
				((SendDataToHW)(0, 0, 4));
			}//如果并非一直都在发送数据，最好在发Stop命令后再发几个数据给色谱工作站程序，以便它跳出等待数据的状态。
			std::cout << "finished" << std::endl;
			FreeLibrary(hinstLib);
		}
	}
	return 0;
}

int modbus_slave() {
	modbus_t *ctx = NULL;
	uint8_t *query;
	int header_length;
	modbus_mapping_t *mb_mapping;

	ctx = modbus_new_rtu("COM1", 115200, 'N', 8, 1);
	modbus_set_slave(ctx, 1);
	query = (uint8_t *)malloc(MODBUS_RTU_MAX_ADU_LENGTH);
	header_length = modbus_get_header_length(ctx);
	modbus_set_debug(ctx, TRUE);

	mb_mapping = modbus_mapping_new_start_address(
			UT_BITS_ADDRESS, UT_BITS_NB,
			UT_INPUT_BITS_ADDRESS, UT_INPUT_BITS_NB,
			UT_REGISTERS_ADDRESS, UT_REGISTERS_NB_MAX,
			UT_INPUT_REGISTERS_ADDRESS, UT_INPUT_REGISTERS_NB);
	if (mb_mapping == NULL) {
		fprintf(stderr, "Failed to allocate the mapping: %s\n",
				modbus_strerror(errno));
		modbus_free(ctx);
		return -1;
	}

	/* Examples from PI_MODBUS_300.pdf.
	   Only the read-only input values are assigned. */

	/* Initialize input values that's can be only done server side. */
	modbus_set_bits_from_bytes(mb_mapping->tab_input_bits, 0, UT_INPUT_BITS_NB,
							   UT_INPUT_BITS_TAB);

	/* Initialize values of INPUT REGISTERS */
	for (int i=0; i < UT_INPUT_REGISTERS_NB; i++) {
		mb_mapping->tab_input_registers[i] = UT_INPUT_REGISTERS_TAB[i];;
	}

	int rc = modbus_connect(ctx);
	if (rc == -1) {
		fprintf(stderr, "Unable to connect %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		return -1;
	}

	for (;;) {
		do {
			rc = modbus_receive(ctx, query);
			/* Filtered queries return 0 */
		} while (rc == 0);

		/* The connection is not closed on errors which require on reply such as
		   bad CRC in RTU. */
		if (rc == -1 && errno != EMBBADCRC) {
			/* Quit */
			break;
		}

		/* Special server behavior to test client */
		if (query[header_length] == 0x03) {
			/* Read holding registers */

			if (MODBUS_GET_INT16_FROM_INT8(query, header_length + 3)
				== UT_REGISTERS_NB_SPECIAL) {
				printf("Set an incorrect number of values\n");
				MODBUS_SET_INT16_TO_INT8(query, header_length + 3,
										 UT_REGISTERS_NB_SPECIAL - 1);
			} else if (MODBUS_GET_INT16_FROM_INT8(query, header_length + 1)
					   == UT_REGISTERS_ADDRESS_SPECIAL) {
				printf("Reply to this special register address by an exception\n");
				modbus_reply_exception(ctx, query,
									   MODBUS_EXCEPTION_SLAVE_OR_SERVER_BUSY);
				continue;
			} else if (MODBUS_GET_INT16_FROM_INT8(query, header_length + 1)
					   == UT_REGISTERS_ADDRESS_INVALID_TID_OR_SLAVE) {
				const int RAW_REQ_LENGTH = 5;
				uint8_t raw_req[] = {
						INVALID_SERVER_ID,
						0x03,
						0x02, 0x00, 0x00
				};

				printf("Reply with an invalid TID or slave\n");
				modbus_send_raw_request(ctx, raw_req, RAW_REQ_LENGTH * sizeof(uint8_t));
				continue;
			} else if (MODBUS_GET_INT16_FROM_INT8(query, header_length + 1)
					   == UT_REGISTERS_ADDRESS_SLEEP_500_MS) {
				printf("Sleep 0.5 s before replying\n");
				Sleep(500);
			} else if (MODBUS_GET_INT16_FROM_INT8(query, header_length + 1)
					   == UT_REGISTERS_ADDRESS_BYTE_SLEEP_5_MS) {
				/* Test low level only available in TCP mode */
				/* Catch the reply and send reply byte a byte */
				uint8_t req[] = "\x00\x1C\x00\x00\x00\x05\xFF\x03\x02\x00\x00";
				int req_length = 11;
				int w_s = modbus_get_socket(ctx);
				if (w_s == -1) {
					fprintf(stderr, "Unable to get a valid socket in special test\n");
					continue;
				}

				/* Copy TID */
				req[1] = query[1];
				for (int i=0; i < req_length; i++) {
					printf("(%.2X)", req[i]);
					Sleep(5);
					rc = send(w_s, (const char*)(req + i), 1, MSG_NOSIGNAL);
					if (rc == -1) {
						break;
					}
				}
				continue;
			}
		}

		rc = modbus_reply(ctx, query, rc, mb_mapping);
		if (rc == -1) {
			break;
		}
	}

	printf("Quit the loop: %s\n", modbus_strerror(errno));

	modbus_mapping_free(mb_mapping);
	free(query);
	/* For RTU */
	modbus_close(ctx);
	modbus_free(ctx);
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
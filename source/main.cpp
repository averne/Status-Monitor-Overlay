#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include "dmntcht.h"

#ifdef CUSTOM
#include "Battery.hpp"
#endif

#define NVGPU_GPU_IOCTL_PMU_GET_GPU_LOAD 0x80044715
#define FieldDescriptor uint32_t

//Common
Thread t0;
Thread t1;
Thread t2;
Thread t3;
Thread t4;
Thread t5;
Thread t6;
Thread t7;
constinit uint64_t systemtickfrequency = 19200000;
bool threadexit = false;
bool threadexit2 = false;
bool Atmosphere_present = false;
uint64_t refreshrate = 1;
FanController g_ICon;

//Mini mode
char Variables[672];

//Checks
Result clkrstCheck = 1;
Result nvCheck = 1;
Result pcvCheck = 1;
Result tsCheck = 1;
Result fanCheck = 1;
Result tcCheck = 1;
Result Hinted = 1;
Result pmdmntCheck = 1;
Result dmntchtCheck = 1;
#ifdef CUSTOM
Result psmCheck = 1;

//Battery
Service* psmService = 0;
BatteryChargeInfoFields* _batteryChargeInfoFields = 0;
char Battery_c[320];
#endif

//Temperatures
int32_t SOC_temperatureC = 0;
int32_t PCB_temperatureC = 0;
int32_t skin_temperaturemiliC = 0;
char SoCPCB_temperature_c[64];
char skin_temperature_c[32];

//CPU Usage
uint64_t idletick0 = 19200000;
uint64_t idletick1 = 19200000;
uint64_t idletick2 = 19200000;
uint64_t idletick3 = 19200000;
char CPU_Usage0[32];
char CPU_Usage1[32];
char CPU_Usage2[32];
char CPU_Usage3[32];
char CPU_compressed_c[160];

//Frequency
///CPU
uint32_t CPU_Hz = 0;
char CPU_Hz_c[32];
///GPU
uint32_t GPU_Hz = 0;
char GPU_Hz_c[32];
///RAM
uint32_t RAM_Hz = 0;
char RAM_Hz_c[32];

//RAM Size
char RAM_all_c[64];
char RAM_application_c[64];
char RAM_applet_c[64];
char RAM_system_c[64];
char RAM_systemunsafe_c[64];
char RAM_compressed_c[320];
char RAM_var_compressed_c[320];
uint64_t RAM_Total_all_u = 0;
uint64_t RAM_Total_application_u = 0;
uint64_t RAM_Total_applet_u = 0;
uint64_t RAM_Total_system_u = 0;
uint64_t RAM_Total_systemunsafe_u = 0;
uint64_t RAM_Used_all_u = 0;
uint64_t RAM_Used_application_u = 0;
uint64_t RAM_Used_applet_u = 0;
uint64_t RAM_Used_system_u = 0;
uint64_t RAM_Used_systemunsafe_u = 0;

//Fan
float Rotation_SpeedLevel_f = 0;
char Rotation_SpeedLevel_c[64];

//GPU Usage
FieldDescriptor fd = 0;
uint32_t GPU_Load_u = 0;
char GPU_Load_c[32];

//NX-FPS
bool GameRunning = false;
bool check = false;
bool SaltySD = false;
uintptr_t FPSaddress = 0x0;
uintptr_t FPSavgaddress = 0x0;
uint64_t PID = 0;
uint8_t FPS = 0xFE;
float FPSavg = 254;
char FPS_c[32];
char FPSavg_c[32];
char FPS_compressed_c[64];
char FPS_var_compressed_c[64];
Handle debug;

//Check if SaltyNX is working
bool CheckPort () {
	Handle saltysd;
	for (int i = 0; i < 67; i++) {
		if (R_SUCCEEDED(svcConnectToNamedPort(&saltysd, "InjectServ"))) {
			svcCloseHandle(saltysd);
			break;
		}
		else {
			if (i == 66) return false;
			svcSleepThread(1'000'000);
		}
	}
	for (int i = 0; i < 67; i++) {
		if (R_SUCCEEDED(svcConnectToNamedPort(&saltysd, "InjectServ"))) {
			svcCloseHandle(saltysd);
			return true;
		}
		else svcSleepThread(1'000'000);
	}
	return false;
}

bool isServiceRunning(const char *serviceName) {	
	Handle handle;	
	SmServiceName service_name = smEncodeName(serviceName);	
	if (R_FAILED(smRegisterService(&handle, service_name, false, 1))) return true;
	else {
		svcCloseHandle(handle);	
		smUnregisterService(service_name);
		return false;
	}
}

void CheckIfGameRunning(void*) {
	while (threadexit2 == false) {
		if (R_FAILED(pmdmntGetApplicationProcessId(&PID))) {
			if (check == false) {
				remove("sdmc:/SaltySD/FPSoffset.hex");
				check = true;
			}
			GameRunning = false;
			svcCloseHandle(debug);
		}
		else if (GameRunning == false) {
			svcSleepThread(1'000'000'000);
			FILE* FPSoffset = fopen("sdmc:/SaltySD/FPSoffset.hex", "rb");
			if (FPSoffset != NULL) {
				if (Atmosphere_present == true) {
					bool out = false;
					dmntchtHasCheatProcess(&out);
					if (out == false) dmntchtForceOpenCheatProcess();
				}
				else svcSleepThread(1'000'000'000);
				fread(&FPSaddress, 0x5, 1, FPSoffset);
				FPSavgaddress = FPSaddress - 0x8;
				fclose(FPSoffset);
				GameRunning = true;
				check = false;
			}
		}
		svcSleepThread(1'000'000'000);
	}
}

//Check for input outside of FPS limitations
void CheckButtons(void*) {
	static uint64_t kHeld = padGetButtons(&pad);
	while (threadexit == false) {
		padUpdate(&pad);
		kHeld = padGetButtons(&pad);
		if ((kHeld & KEY_ZR) && (kHeld & KEY_R)) {
			if (kHeld & KEY_DDOWN) {
				TeslaFPS = 1;
				refreshrate = 1;
				systemtickfrequency = 19200000;
			}
			else if (kHeld & KEY_DUP) {
				TeslaFPS = 5;
				refreshrate = 5;
				systemtickfrequency = 3840000;
			}
		}
		svcSleepThread(100'000'000);
	}
}

//Stuff that doesn't need multithreading
void Misc(void*) {
	while (threadexit == false) {
		
		// CPU, GPU and RAM Frequency
		if (R_SUCCEEDED(clkrstCheck)) {
			ClkrstSession clkSession;
			if (R_SUCCEEDED(clkrstOpenSession(&clkSession, PcvModuleId_CpuBus, 3))) {
				clkrstGetClockRate(&clkSession, &CPU_Hz);
				clkrstCloseSession(&clkSession);
			}
			if (R_SUCCEEDED(clkrstOpenSession(&clkSession, PcvModuleId_GPU, 3))) {
				clkrstGetClockRate(&clkSession, &GPU_Hz);
				clkrstCloseSession(&clkSession);
			}
			if (R_SUCCEEDED(clkrstOpenSession(&clkSession, PcvModuleId_EMC, 3))) {
				clkrstGetClockRate(&clkSession, &RAM_Hz);
				clkrstCloseSession(&clkSession);
			}
		}
		else if (R_SUCCEEDED(pcvCheck)) {
			pcvGetClockRate(PcvModule_CpuBus, &CPU_Hz);
			pcvGetClockRate(PcvModule_GPU, &GPU_Hz);
			pcvGetClockRate(PcvModule_EMC, &RAM_Hz);
		}
		
		//Temperatures
		if (R_SUCCEEDED(tsCheck)) {
			if (hosversionAtLeast(14,0,0)) {
				tsGetTemperature(TsLocation_External, &SOC_temperatureC);
				tsGetTemperature(TsLocation_Internal, &PCB_temperatureC);
			}
			else {
				tsGetTemperatureMilliC(TsLocation_External, &SOC_temperatureC);
				tsGetTemperatureMilliC(TsLocation_Internal, &PCB_temperatureC);
			}
		}
		if (R_SUCCEEDED(tcCheck)) tcGetSkinTemperatureMilliC(&skin_temperaturemiliC);
		
		//RAM Memory Used
		if (R_SUCCEEDED(Hinted)) {
			svcGetSystemInfo(&RAM_Total_application_u, 0, INVALID_HANDLE, 0);
			svcGetSystemInfo(&RAM_Total_applet_u, 0, INVALID_HANDLE, 1);
			svcGetSystemInfo(&RAM_Total_system_u, 0, INVALID_HANDLE, 2);
			svcGetSystemInfo(&RAM_Total_systemunsafe_u, 0, INVALID_HANDLE, 3);
			svcGetSystemInfo(&RAM_Used_application_u, 1, INVALID_HANDLE, 0);
			svcGetSystemInfo(&RAM_Used_applet_u, 1, INVALID_HANDLE, 1);
			svcGetSystemInfo(&RAM_Used_system_u, 1, INVALID_HANDLE, 2);
			svcGetSystemInfo(&RAM_Used_systemunsafe_u, 1, INVALID_HANDLE, 3);
		}
		
		//Fan
		if (R_SUCCEEDED(fanCheck)) fanControllerGetRotationSpeedLevel(&g_ICon, &Rotation_SpeedLevel_f);
		
		//GPU Load
		if (R_SUCCEEDED(nvCheck)) nvIoctl(fd, NVGPU_GPU_IOCTL_PMU_GET_GPU_LOAD, &GPU_Load_u);
		
		//FPS
		if (GameRunning == true) {
			if (Atmosphere_present == true) {
				dmntchtReadCheatProcessMemory(FPSaddress, &FPS, 0x1);
				dmntchtReadCheatProcessMemory(FPSavgaddress, &FPSavg, 0x4);
			}
			else if (R_SUCCEEDED(svcDebugActiveProcess(&debug, PID))) {
				svcReadDebugProcessMemory(&FPS, debug, FPSaddress, 0x1);
				svcReadDebugProcessMemory(&FPSavg, debug, FPSavgaddress, 0x4);
				svcCloseHandle(debug);
			}
		}
		
		// Interval
		svcSleepThread(1'000'000'000 / refreshrate);
	}
}

//Check each core for idled ticks in intervals, they cannot read info about other core than they are assigned
void CheckCore0(void*) {
	while (threadexit == false) {
		static uint64_t idletick_a0 = 0;
		static uint64_t idletick_b0 = 0;
		svcGetInfo(&idletick_b0, InfoType_IdleTickCount, INVALID_HANDLE, 0);
		svcSleepThread(1'000'000'000 / refreshrate);
		svcGetInfo(&idletick_a0, InfoType_IdleTickCount, INVALID_HANDLE, 0);
		idletick0 = idletick_a0 - idletick_b0;
	}
}

void CheckCore1(void*) {
	while (threadexit == false) {
		static uint64_t idletick_a1 = 0;
		static uint64_t idletick_b1 = 0;
		svcGetInfo(&idletick_b1, InfoType_IdleTickCount, INVALID_HANDLE, 1);
		svcSleepThread(1'000'000'000 / refreshrate);
		svcGetInfo(&idletick_a1, InfoType_IdleTickCount, INVALID_HANDLE, 1);
		idletick1 = idletick_a1 - idletick_b1;
	}
}

void CheckCore2(void*) {
	while (threadexit == false) {
		static uint64_t idletick_a2 = 0;
		static uint64_t idletick_b2 = 0;
		svcGetInfo(&idletick_b2, InfoType_IdleTickCount, INVALID_HANDLE, 2);
		svcSleepThread(1'000'000'000 / refreshrate);
		svcGetInfo(&idletick_a2, InfoType_IdleTickCount, INVALID_HANDLE, 2);
		idletick2 = idletick_a2 - idletick_b2;
	}
}

void CheckCore3(void*) {
	while (threadexit == false) {
		static uint64_t idletick_a3 = 0;
		static uint64_t idletick_b3 = 0;
		svcGetInfo(&idletick_b3, InfoType_IdleTickCount, INVALID_HANDLE, 3);
		svcSleepThread(1'000'000'000 / refreshrate);
		svcGetInfo(&idletick_a3, InfoType_IdleTickCount, INVALID_HANDLE, 3);
		idletick3 = idletick_a3 - idletick_b3;
		
	}
}

//Start reading all stats
void StartThreads() {
	threadCreate(&t0, CheckCore0, NULL, NULL, 0x100, 0x10, 0);
	threadCreate(&t1, CheckCore1, NULL, NULL, 0x100, 0x10, 1);
	threadCreate(&t2, CheckCore2, NULL, NULL, 0x100, 0x10, 2);
	threadCreate(&t3, CheckCore3, NULL, NULL, 0x100, 0x10, 3);
	threadCreate(&t4, Misc, NULL, NULL, 0x100, 0x3F, -2);
	threadCreate(&t5, CheckButtons, NULL, NULL, 0x400, 0x3F, -2);
	threadStart(&t0);
	threadStart(&t1);
	threadStart(&t2);
	threadStart(&t3);
	threadStart(&t4);
	threadStart(&t5);
}

//End reading all stats
void CloseThreads() {
	threadexit = true;
	threadexit2 = true;
	threadWaitForExit(&t0);
	threadWaitForExit(&t1);
	threadWaitForExit(&t2);
	threadWaitForExit(&t3);
	threadWaitForExit(&t4);
	threadWaitForExit(&t5);
	threadWaitForExit(&t6);
	threadWaitForExit(&t7);
	threadClose(&t0);
	threadClose(&t1);
	threadClose(&t2);
	threadClose(&t3);
	threadClose(&t4);
	threadClose(&t5);
	threadClose(&t6);
	threadClose(&t7);
	threadexit = false;
	threadexit2 = false;
}

//Separate functions dedicated to "FPS Counter" mode
void FPSCounter(void*) {
	while (threadexit == false) {
		if (GameRunning == true) {
			if (Atmosphere_present == true) dmntchtReadCheatProcessMemory(FPSavgaddress, &FPSavg, 0x4);
			else if (R_SUCCEEDED(svcDebugActiveProcess(&debug, PID))) {
				svcReadDebugProcessMemory(&FPSavg, debug, FPSavgaddress, 0x4);
				svcCloseHandle(debug);
			}
		}
		else FPSavg = 254;
		//interval
		svcSleepThread(1'000'000'000 / refreshrate);
	}
}

void StartFPSCounterThread() {
	threadCreate(&t0, FPSCounter, NULL, NULL, 0x1000, 0x3F, 3);
	threadStart(&t0);
}

void EndFPSCounterThread() {
	threadexit = true;
	threadWaitForExit(&t0);
	threadClose(&t0);
	threadexit = false;
}

//FPS Counter mode
class com_FPS : public tsl::Gui {
public:
    com_FPS() { }

    virtual tsl::elm::Element* createUI() override {
		auto rootFrame = new tsl::elm::OverlayFrame("", "");

		auto Status = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, u16 x, u16 y, u16 w, u16 h) {
				static uint8_t avg = 0;
				if (FPSavg < 10) avg = 0;
				if (FPSavg >= 10) avg = 23;
				if (FPSavg >= 100) avg = 46;
				renderer->drawRect(0, 0, tsl::cfg::FramebufferWidth - 370 + avg, 50, a(0x7111));
				renderer->drawString(FPSavg_c, false, 5, 40, 40, renderer->a(0xFFFF));
		});

		rootFrame->setContent(Status);

		return rootFrame;
	}

	virtual void update() override {
		///FPS
		snprintf(FPSavg_c, sizeof FPSavg_c, "%2.1f", FPSavg);
		
	}
	virtual bool handleInput(uint64_t keysDown, uint64_t keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
		if ((keysHeld & KEY_LSTICK) && (keysHeld & KEY_RSTICK)) {
			EndFPSCounterThread();
			tsl::goBack();
			return true;
		}
		return false;
	}
};

//Full mode
class FullOverlay : public tsl::Gui {
public:
    FullOverlay() { }

    virtual tsl::elm::Element* createUI() override {
		auto rootFrame = new tsl::elm::OverlayFrame("Status Monitor", APP_VERSION);

		auto Status = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, u16 x, u16 y, u16 w, u16 h) {
			
			//Print strings
			///CPU
			if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck)) {
				renderer->drawString("CPU Usage:", false, 20, 120, 20, renderer->a(0xFFFF));
				renderer->drawString(CPU_Hz_c, false, 20, 155, 15, renderer->a(0xFFFF));
				renderer->drawString(CPU_compressed_c, false, 20, 185, 15, renderer->a(0xFFFF));
			}
			
			///GPU
			if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck) || R_SUCCEEDED(nvCheck)) {
				
				renderer->drawString("GPU Usage:", false, 20, 285, 20, renderer->a(0xFFFF));
				if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck)) renderer->drawString(GPU_Hz_c, false, 20, 320, 15, renderer->a(0xFFFF));
				if (R_SUCCEEDED(nvCheck)) renderer->drawString(GPU_Load_c, false, 20, 335, 15, renderer->a(0xFFFF));
				
			}
			
			///RAM
			if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck) || R_SUCCEEDED(Hinted)) {
				
				renderer->drawString("RAM Usage:", false, 20, 375, 20, renderer->a(0xFFFF));
				if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck)) renderer->drawString(RAM_Hz_c, false, 20, 410, 15, renderer->a(0xFFFF));
				if (R_SUCCEEDED(Hinted)) {
					renderer->drawString(RAM_compressed_c, false, 20, 440, 15, renderer->a(0xFFFF));
					renderer->drawString(RAM_var_compressed_c, false, 140, 440, 15, renderer->a(0xFFFF));
				}
			}
			
			///Thermal
			if (R_SUCCEEDED(tsCheck) || R_SUCCEEDED(tcCheck) || R_SUCCEEDED(fanCheck)) {
				renderer->drawString("Thermal:", false, 20, 540, 20, renderer->a(0xFFFF));
				if (R_SUCCEEDED(tsCheck)) renderer->drawString(SoCPCB_temperature_c, false, 20, 575, 15, renderer->a(0xFFFF));
				if (R_SUCCEEDED(tcCheck)) renderer->drawString(skin_temperature_c, false, 20, 605, 15, renderer->a(0xFFFF));
				if (R_SUCCEEDED(fanCheck)) renderer->drawString(Rotation_SpeedLevel_c, false, 20, 620, 15, renderer->a(0xFFFF));
			}
			
			///FPS
			if (GameRunning == true) {
				renderer->drawString(FPS_compressed_c, false, 235, 120, 20, renderer->a(0xFFFF));
				renderer->drawString(FPS_var_compressed_c, false, 295, 120, 20, renderer->a(0xFFFF));
			}
			
			if (refreshrate == 5) renderer->drawString("Hold Left Stick & Right Stick to Exit\nHold ZR + R + D-Pad Down to slow down refresh", false, 20, 675, 15, renderer->a(0xFFFF));
			else if (refreshrate == 1) renderer->drawString("Hold Left Stick & Right Stick to Exit\nHold ZR + R + D-Pad Up to speed up refresh", false, 20, 675, 15, renderer->a(0xFFFF));
		
		});

		rootFrame->setContent(Status);

		return rootFrame;
	}

	virtual void update() override {
		if (TeslaFPS == 60) TeslaFPS = 1;
		//In case of getting more than systemtickfrequency in idle, make it equal to systemtickfrequency to get 0% as output and nothing less
		//This is because making each loop also takes time, which is not considered because this will take also additional time
		if (idletick0 > systemtickfrequency) idletick0 = systemtickfrequency;
		if (idletick1 > systemtickfrequency) idletick1 = systemtickfrequency;
		if (idletick2 > systemtickfrequency) idletick2 = systemtickfrequency;
		if (idletick3 > systemtickfrequency) idletick3 = systemtickfrequency;
		
		//Make stuff ready to print
		///CPU
		snprintf(CPU_Hz_c, sizeof CPU_Hz_c, "Frequency: %.1f MHz", (float)CPU_Hz / 1000000);
		snprintf(CPU_Usage0, sizeof CPU_Usage0, "Core #0: %.2f%s", ((double)systemtickfrequency - (double)idletick0) / (double)systemtickfrequency * 100, "%");
		snprintf(CPU_Usage1, sizeof CPU_Usage1, "Core #1: %.2f%s", ((double)systemtickfrequency - (double)idletick1) / (double)systemtickfrequency * 100, "%");
		snprintf(CPU_Usage2, sizeof CPU_Usage2, "Core #2: %.2f%s", ((double)systemtickfrequency - (double)idletick2) / (double)systemtickfrequency * 100, "%");
		snprintf(CPU_Usage3, sizeof CPU_Usage3, "Core #3: %.2f%s", ((double)systemtickfrequency - (double)idletick3) / (double)systemtickfrequency * 100, "%");
		snprintf(CPU_compressed_c, sizeof CPU_compressed_c, "%s\n%s\n%s\n%s", CPU_Usage0, CPU_Usage1, CPU_Usage2, CPU_Usage3);
		
		///GPU
		snprintf(GPU_Hz_c, sizeof GPU_Hz_c, "Frequency: %.1f MHz", (float)GPU_Hz / 1000000);
		snprintf(GPU_Load_c, sizeof GPU_Load_c, "Load: %.1f%s", (float)GPU_Load_u / 10, "%");
		
		///RAM
		snprintf(RAM_Hz_c, sizeof RAM_Hz_c, "Frequency: %.1f MHz", (float)RAM_Hz / 1000000);
		float RAM_Total_application_f = (float)RAM_Total_application_u / 1024 / 1024;
		float RAM_Total_applet_f = (float)RAM_Total_applet_u / 1024 / 1024;
		float RAM_Total_system_f = (float)RAM_Total_system_u / 1024 / 1024;
		float RAM_Total_systemunsafe_f = (float)RAM_Total_systemunsafe_u / 1024 / 1024;
		float RAM_Total_all_f = RAM_Total_application_f + RAM_Total_applet_f + RAM_Total_system_f + RAM_Total_systemunsafe_f;
		float RAM_Used_application_f = (float)RAM_Used_application_u / 1024 / 1024;
		float RAM_Used_applet_f = (float)RAM_Used_applet_u / 1024 / 1024;
		float RAM_Used_system_f = (float)RAM_Used_system_u / 1024 / 1024;
		float RAM_Used_systemunsafe_f = (float)RAM_Used_systemunsafe_u / 1024 / 1024;
		float RAM_Used_all_f = RAM_Used_application_f + RAM_Used_applet_f + RAM_Used_system_f + RAM_Used_systemunsafe_f;
		snprintf(RAM_all_c, sizeof RAM_all_c, "Total:");
		snprintf(RAM_application_c, sizeof RAM_application_c, "Application:");
		snprintf(RAM_applet_c, sizeof RAM_applet_c, "Applet:");
		snprintf(RAM_system_c, sizeof RAM_system_c, "System:");
		snprintf(RAM_systemunsafe_c, sizeof RAM_systemunsafe_c, "System Unsafe:");
		snprintf(RAM_compressed_c, sizeof RAM_compressed_c, "%s\n%s\n%s\n%s\n%s", RAM_all_c, RAM_application_c, RAM_applet_c, RAM_system_c, RAM_systemunsafe_c);
		snprintf(RAM_all_c, sizeof RAM_all_c, "%4.2f / %4.2f MB", RAM_Used_all_f, RAM_Total_all_f);
		snprintf(RAM_application_c, sizeof RAM_application_c, "%4.2f / %4.2f MB", RAM_Used_application_f, RAM_Total_application_f);
		snprintf(RAM_applet_c, sizeof RAM_applet_c, "%4.2f / %4.2f MB", RAM_Used_applet_f, RAM_Total_applet_f);
		snprintf(RAM_system_c, sizeof RAM_system_c, "%4.2f / %4.2f MB", RAM_Used_system_f, RAM_Total_system_f);
		snprintf(RAM_systemunsafe_c, sizeof RAM_systemunsafe_c, "%4.2f / %4.2f MB", RAM_Used_systemunsafe_f, RAM_Total_systemunsafe_f);
		snprintf(RAM_var_compressed_c, sizeof RAM_var_compressed_c, "%s\n%s\n%s\n%s\n%s", RAM_all_c, RAM_application_c, RAM_applet_c, RAM_system_c, RAM_systemunsafe_c);
		
		///Thermal
		if (hosversionAtLeast(14,0,0))
			snprintf(SoCPCB_temperature_c, sizeof SoCPCB_temperature_c, "SoC: %2d \u00B0C\nPCB: %2d \u00B0C", SOC_temperatureC, PCB_temperatureC);
		else 
			snprintf(SoCPCB_temperature_c, sizeof SoCPCB_temperature_c, "SoC: %2.2f \u00B0C\nPCB: %2.2f \u00B0C", (float)SOC_temperatureC / 1000, (float)PCB_temperatureC / 1000);
		snprintf(skin_temperature_c, sizeof skin_temperature_c, "Skin: %2.2f \u00B0C", (float)skin_temperaturemiliC / 1000);
		snprintf(Rotation_SpeedLevel_c, sizeof Rotation_SpeedLevel_c, "Fan: %2.2f%s", Rotation_SpeedLevel_f * 100, "%");
		
		///FPS
		snprintf(FPS_c, sizeof FPS_c, "PFPS:"); //Pushed Frames Per Second
		snprintf(FPSavg_c, sizeof FPSavg_c, "FPS:"); //Frames Per Second calculated from averaged frametime 
		snprintf(FPS_compressed_c, sizeof FPS_compressed_c, "%s\n%s", FPS_c, FPSavg_c);
		snprintf(FPS_var_compressed_c, sizeof FPS_var_compressed_c, "%u\n%2.2f", FPS, FPSavg);
		
	}
	virtual bool handleInput(uint64_t keysDown, uint64_t keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
		if ((keysHeld & KEY_LSTICK) && (keysHeld & KEY_RSTICK)) {
			CloseThreads();
			tsl::goBack();
			return true;
		}
		return false;
	}
};

//Mini mode
class MiniOverlay : public tsl::Gui {
public:
    MiniOverlay() { }

    virtual tsl::elm::Element* createUI() override {
		
		auto rootFrame = new tsl::elm::OverlayFrame("", "");

		auto Status = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, u16 x, u16 y, u16 w, u16 h) {
			
			if (GameRunning == false) renderer->drawRect(0, 0, tsl::cfg::FramebufferWidth - 150, 80, a(0x7111));
			else renderer->drawRect(0, 0, tsl::cfg::FramebufferWidth - 150, 110, a(0x7111));
			
			//Print strings
			///CPU
			if (GameRunning == true) renderer->drawString("CPU\nGPU\nRAM\nTEMP\nFAN\nPFPS\nFPS", false, 0, 15, 15, renderer->a(0xFFFF));
			else renderer->drawString("CPU\nGPU\nRAM\nTEMP\nFAN", false, 0, 15, 15, renderer->a(0xFFFF));
			
			///GPU
			renderer->drawString(Variables, false, 60, 15, 15, renderer->a(0xFFFF));
		});

		rootFrame->setContent(Status);

		return rootFrame;
	}

	virtual void update() override {
		if (TeslaFPS == 60) TeslaFPS = 1;
		//In case of getting more than systemtickfrequency in idle, make it equal to systemtickfrequency to get 0% as output and nothing less
		//This is because making each loop also takes time, which is not considered because this will take also additional time
		if (idletick0 > systemtickfrequency) idletick0 = systemtickfrequency;
		if (idletick1 > systemtickfrequency) idletick1 = systemtickfrequency;
		if (idletick2 > systemtickfrequency) idletick2 = systemtickfrequency;
		if (idletick3 > systemtickfrequency) idletick3 = systemtickfrequency;
		
		//Make stuff ready to print
		///CPU
		double percent = ((double)systemtickfrequency - (double)idletick0) / (double)systemtickfrequency * 100;
		snprintf(CPU_Usage0, sizeof CPU_Usage0, "%.0f%s", percent, "%");
		percent = ((double)systemtickfrequency - (double)idletick1) / (double)systemtickfrequency * 100;
		snprintf(CPU_Usage1, sizeof CPU_Usage1, "%.0f%s", percent, "%");
		percent = ((double)systemtickfrequency - (double)idletick2) / (double)systemtickfrequency * 100;
		snprintf(CPU_Usage2, sizeof CPU_Usage2, "%.0f%s", percent, "%");
		percent = ((double)systemtickfrequency - (double)idletick3) / (double)systemtickfrequency * 100;
		snprintf(CPU_Usage3, sizeof CPU_Usage3, "%.0f%s", percent, "%");
		snprintf(CPU_compressed_c, sizeof CPU_compressed_c, "[%s,%s,%s,%s]@%.1f", CPU_Usage0, CPU_Usage1, CPU_Usage2, CPU_Usage3, (float)CPU_Hz / 1000000);
		
		///GPU
		snprintf(GPU_Load_c, sizeof GPU_Load_c, "%.1f%s@%.1f", (float)GPU_Load_u / 10, "%", (float)GPU_Hz / 1000000);
		
		///RAM
		float RAM_Total_application_f = (float)RAM_Total_application_u / 1024 / 1024;
		float RAM_Total_applet_f = (float)RAM_Total_applet_u / 1024 / 1024;
		float RAM_Total_system_f = (float)RAM_Total_system_u / 1024 / 1024;
		float RAM_Total_systemunsafe_f = (float)RAM_Total_systemunsafe_u / 1024 / 1024;
		float RAM_Total_all_f = RAM_Total_application_f + RAM_Total_applet_f + RAM_Total_system_f + RAM_Total_systemunsafe_f;
		float RAM_Used_application_f = (float)RAM_Used_application_u / 1024 / 1024;
		float RAM_Used_applet_f = (float)RAM_Used_applet_u / 1024 / 1024;
		float RAM_Used_system_f = (float)RAM_Used_system_u / 1024 / 1024;
		float RAM_Used_systemunsafe_f = (float)RAM_Used_systemunsafe_u / 1024 / 1024;
		float RAM_Used_all_f = RAM_Used_application_f + RAM_Used_applet_f + RAM_Used_system_f + RAM_Used_systemunsafe_f;
		snprintf(RAM_all_c, sizeof RAM_all_c, "%.0f/%.0fMB", RAM_Used_all_f, RAM_Total_all_f);
		snprintf(RAM_var_compressed_c, sizeof RAM_var_compressed_c, "%s@%.1f", RAM_all_c, (float)RAM_Hz / 1000000);
		
		///Thermal
		if (hosversionAtLeast(14,0,0))
			snprintf(skin_temperature_c, sizeof skin_temperature_c, "%2d\u00B0C/%2d\u00B0C/%2.1f\u00B0C", SOC_temperatureC, PCB_temperatureC, (float)skin_temperaturemiliC / 1000);
		else
			snprintf(skin_temperature_c, sizeof skin_temperature_c, "%2.1f\u00B0C/%2.1f\u00B0C/%2.1f\u00B0C", (float)SOC_temperatureC / 1000, (float)PCB_temperatureC / 1000, (float)skin_temperaturemiliC / 1000);
		snprintf(Rotation_SpeedLevel_c, sizeof Rotation_SpeedLevel_c, "%2.2f%s", Rotation_SpeedLevel_f * 100, "%");
		
		///FPS
		snprintf(FPS_c, sizeof FPS_c, "PFPS:"); //Pushed Frames Per Second
		snprintf(FPSavg_c, sizeof FPSavg_c, "FPS:"); //Frames Per Second calculated from averaged frametime 
		snprintf(FPS_compressed_c, sizeof FPS_compressed_c, "%s\n%s", FPS_c, FPSavg_c);
		snprintf(FPS_var_compressed_c, sizeof FPS_compressed_c, "%u\n%2.2f", FPS, FPSavg);

		if (GameRunning == true) snprintf(Variables, sizeof Variables, "%s\n%s\n%s\n%s\n%s\n%s", CPU_compressed_c, GPU_Load_c, RAM_var_compressed_c, skin_temperature_c, Rotation_SpeedLevel_c, FPS_var_compressed_c);
		else snprintf(Variables, sizeof Variables, "%s\n%s\n%s\n%s\n%s", CPU_compressed_c, GPU_Load_c, RAM_var_compressed_c, skin_temperature_c, Rotation_SpeedLevel_c);

	}
	virtual bool handleInput(uint64_t keysDown, uint64_t keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
		if ((keysHeld & KEY_LSTICK) && (keysHeld & KEY_RSTICK)) {
			CloseThreads();
			tsl::goBack();
			return true;
		}
		return false;
	}
};

#ifdef CUSTOM
void BatteryChecker(void*) {
	if (R_SUCCEEDED(psmCheck)){
		_batteryChargeInfoFields = new BatteryChargeInfoFields;
		while (!threadexit) {
			psmGetBatteryChargeInfoFields(psmService, _batteryChargeInfoFields);
			svcSleepThread(5'000'000'000);
		}
		delete _batteryChargeInfoFields;
	}
}

void StartBatteryThread() {
	threadCreate(&t7, BatteryChecker, NULL, NULL, 0x4000, 0x3F, 3);
	threadStart(&t7);
}

//CustomOverlay
class CustomOverlay : public tsl::Gui {
public:
    CustomOverlay() { }

    virtual tsl::elm::Element* createUI() override {
		auto rootFrame = new tsl::elm::OverlayFrame("Status Monitor", APP_VERSION);

		auto Status = new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *renderer, u16 x, u16 y, u16 w, u16 h) {
			
			//Print strings
			///CPU
			if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck)) {
				renderer->drawString("CPU Usage:", false, 20, 120, 20, renderer->a(0xFFFF));
				renderer->drawString(CPU_Hz_c, false, 20, 155, 15, renderer->a(0xFFFF));
				renderer->drawString(CPU_compressed_c, false, 20, 185, 15, renderer->a(0xFFFF));
			}
			
			///GPU
			if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck) || R_SUCCEEDED(nvCheck)) {
				
				renderer->drawString("GPU Usage:", false, 20, 285, 20, renderer->a(0xFFFF));
				if (R_SUCCEEDED(clkrstCheck) || R_SUCCEEDED(pcvCheck)) renderer->drawString(GPU_Hz_c, false, 20, 320, 15, renderer->a(0xFFFF));
				if (R_SUCCEEDED(nvCheck)) renderer->drawString(GPU_Load_c, false, 20, 335, 15, renderer->a(0xFFFF));
				
			}
			
			///RAM
			if (R_SUCCEEDED(psmCheck)) {
				
				renderer->drawString("Battery Stats:", false, 20, 375, 20, renderer->a(0xFFFF));
				renderer->drawString(Battery_c, false, 20, 410, 15, renderer->a(0xFFFF));
			}
			
			///Thermal
			if (R_SUCCEEDED(tsCheck) || R_SUCCEEDED(tcCheck) || R_SUCCEEDED(fanCheck)) {
				renderer->drawString("Thermal:", false, 20, 540, 20, renderer->a(0xFFFF));
				if (R_SUCCEEDED(tsCheck)) renderer->drawString(SoCPCB_temperature_c, false, 20, 575, 15, renderer->a(0xFFFF));
				if (R_SUCCEEDED(tcCheck)) renderer->drawString(skin_temperature_c, false, 20, 605, 15, renderer->a(0xFFFF));
				if (R_SUCCEEDED(fanCheck)) renderer->drawString(Rotation_SpeedLevel_c, false, 20, 620, 15, renderer->a(0xFFFF));
			}
			
			if (refreshrate == 5) renderer->drawString("Hold Left Stick & Right Stick to Exit\nHold ZR + R + D-Pad Down to slow down refresh", false, 20, 675, 15, renderer->a(0xFFFF));
			else if (refreshrate == 1) renderer->drawString("Hold Left Stick & Right Stick to Exit\nHold ZR + R + D-Pad Up to speed up refresh", false, 20, 675, 15, renderer->a(0xFFFF));
		
		});

		rootFrame->setContent(Status);

		return rootFrame;
	}

	virtual void update() override {
		if (TeslaFPS == 60) TeslaFPS = 1;
		//In case of getting more than systemtickfrequency in idle, make it equal to systemtickfrequency to get 0% as output and nothing less
		//This is because making each loop also takes time, which is not considered because this will take also additional time
		if (idletick0 > systemtickfrequency) idletick0 = systemtickfrequency;
		if (idletick1 > systemtickfrequency) idletick1 = systemtickfrequency;
		if (idletick2 > systemtickfrequency) idletick2 = systemtickfrequency;
		if (idletick3 > systemtickfrequency) idletick3 = systemtickfrequency;
		
		//Make stuff ready to print
		///CPU
		snprintf(CPU_Hz_c, sizeof CPU_Hz_c, "Frequency: %.1f MHz", (float)CPU_Hz / 1000000);
		snprintf(CPU_Usage0, sizeof CPU_Usage0, "Core #0: %.2f%s", ((double)systemtickfrequency - (double)idletick0) / (double)systemtickfrequency * 100, "%");
		snprintf(CPU_Usage1, sizeof CPU_Usage1, "Core #1: %.2f%s", ((double)systemtickfrequency - (double)idletick1) / (double)systemtickfrequency * 100, "%");
		snprintf(CPU_Usage2, sizeof CPU_Usage2, "Core #2: %.2f%s", ((double)systemtickfrequency - (double)idletick2) / (double)systemtickfrequency * 100, "%");
		snprintf(CPU_Usage3, sizeof CPU_Usage3, "Core #3: %.2f%s", ((double)systemtickfrequency - (double)idletick3) / (double)systemtickfrequency * 100, "%");
		snprintf(CPU_compressed_c, sizeof CPU_compressed_c, "%s\n%s\n%s\n%s", CPU_Usage0, CPU_Usage1, CPU_Usage2, CPU_Usage3);
		
		///GPU
		snprintf(GPU_Hz_c, sizeof GPU_Hz_c, "Frequency: %.1f MHz", (float)GPU_Hz / 1000000);
		snprintf(GPU_Load_c, sizeof GPU_Load_c, "Load: %.1f%s", (float)GPU_Load_u / 10, "%");
		
		///Battery
		snprintf(Battery_c, sizeof Battery_c,
			"Battery Temperature: %.1f\u00B0C\n"
			"Raw Battery Charge: %.1f%s\n"
			"Voltage Avg: %u mV\n"
			"Charger Type: %u\n",
			(float)_batteryChargeInfoFields->BatteryTemperature / 1000,
			(float)_batteryChargeInfoFields->RawBatteryCharge / 1000, "%",
			_batteryChargeInfoFields->VoltageAvg,
			_batteryChargeInfoFields->ChargerType
		);
		
		///Thermal
		if (hosversionAtLeast(14,0,0))
			snprintf(skin_temperature_c, sizeof skin_temperature_c, "%2d\u00B0C/%2d\u00B0C/%2.1f\u00B0C", SOC_temperatureC, PCB_temperatureC, (float)skin_temperaturemiliC / 1000);
		else
			snprintf(skin_temperature_c, sizeof skin_temperature_c, "%2.1f\u00B0C/%2.1f\u00B0C/%2.1f\u00B0C", (float)SOC_temperatureC / 1000, (float)PCB_temperatureC / 1000, (float)skin_temperaturemiliC / 1000);
		snprintf(skin_temperature_c, sizeof skin_temperature_c, "Skin: %2.2f \u00B0C", (float)skin_temperaturemiliC / 1000);
		snprintf(Rotation_SpeedLevel_c, sizeof Rotation_SpeedLevel_c, "Fan: %2.2f%s", Rotation_SpeedLevel_f * 100, "%");
		
	}
	virtual bool handleInput(uint64_t keysDown, uint64_t keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
		if ((keysHeld & KEY_LSTICK) && (keysHeld & KEY_RSTICK)) {
			CloseThreads();
			tsl::goBack();
			return true;
		}
		return false;
	}
};
#endif

//Main Menu
class MainMenu : public tsl::Gui {
public:
    MainMenu() { }

    virtual tsl::elm::Element* createUI() override {
		auto rootFrame = new tsl::elm::OverlayFrame("Status Monitor", APP_VERSION);
		auto list = new tsl::elm::List();
		
		auto Full = new tsl::elm::ListItem("Full");
		Full->setClickListener([](uint64_t keys) {
			if (keys & KEY_A) {
				StartThreads();
				TeslaFPS = 1;
				refreshrate = 1;
				tsl::hlp::requestForeground(false);
				tsl::changeTo<FullOverlay>();
				return true;
			}
			return false;
		});
		list->addItem(Full);
		auto Mini = new tsl::elm::ListItem("Mini");
		Mini->setClickListener([](uint64_t keys) {
			if (keys & KEY_A) {
				StartThreads();
				TeslaFPS = 1;
				refreshrate = 1;
				alphabackground = 0x0;
				tsl::hlp::requestForeground(false);
				FullMode = false;
				tsl::changeTo<MiniOverlay>();
				return true;
			}
			return false;
		});
		list->addItem(Mini);
		if (SaltySD == true) {
			auto comFPS = new tsl::elm::ListItem("FPS Counter");
			comFPS->setClickListener([](uint64_t keys) {
				if (keys & KEY_A) {
					StartFPSCounterThread();
					TeslaFPS = 31;
					refreshrate = 31;
					alphabackground = 0x0;
					tsl::hlp::requestForeground(false);
					FullMode = false;
					tsl::changeTo<com_FPS>();
					return true;
				}
				return false;
			});
			list->addItem(comFPS);
		}
#ifdef CUSTOM
		auto Custom = new tsl::elm::ListItem("Custom");
		Custom->setClickListener([](uint64_t keys) {
			if (keys & KEY_A) {
				StartThreads();
				StartBatteryThread();
				TeslaFPS = 1;
				refreshrate = 1;
				tsl::hlp::requestForeground(false);
				tsl::changeTo<CustomOverlay>();
				return true;
			}
			return false;
		});
		list->addItem(Custom);
#endif

		rootFrame->setContent(list);

		return rootFrame;
	}

	virtual void update() override {
		if (TeslaFPS != 60) {
			FullMode = true;
			tsl::hlp::requestForeground(true);
			TeslaFPS = 60;
			alphabackground = 0xD;
			refreshrate = 1;
			systemtickfrequency = 19200000;
		}
	}
    virtual bool handleInput(uint64_t keysDown, uint64_t keysHeld, touchPosition touchInput, JoystickPosition leftJoyStick, JoystickPosition rightJoyStick) override {
		if (keysHeld & KEY_B) {
			tsl::goBack();
			return true;
		}
		return false;
    }
};

class MonitorOverlay : public tsl::Overlay {
public:

	virtual void initServices() override {
		//Initialize services
		if (R_SUCCEEDED(smInitialize())) {
			
			if (hosversionAtLeast(8,0,0)) clkrstCheck = clkrstInitialize();
			else pcvCheck = pcvInitialize();
			
			tsCheck = tsInitialize();
			if (hosversionAtLeast(5,0,0)) tcCheck = tcInitialize();

			if (R_SUCCEEDED(fanInitialize())) {
				if (hosversionAtLeast(7,0,0)) fanCheck = fanOpenController(&g_ICon, 0x3D000001);
				else fanCheck = fanOpenController(&g_ICon, 1);
			}

			if (R_SUCCEEDED(nvInitialize())) nvCheck = nvOpen(&fd, "/dev/nvhost-ctrl-gpu");
			
#ifdef CUSTOM
			psmCheck = psmInitialize();
			if (R_SUCCEEDED(psmCheck)) psmService = psmGetServiceSession();
#endif
			
			Atmosphere_present = isServiceRunning("dmnt:cht");
			SaltySD = CheckPort();
			if (SaltySD == true && Atmosphere_present == true) dmntchtCheck = dmntchtInitialize();
			
			if (SaltySD == true) {
				//Assign NX-FPS to default core
				threadCreate(&t6, CheckIfGameRunning, NULL, NULL, 0x1000, 0x38, -2);
				
				//Start NX-FPS detection
				threadStart(&t6);
			}
			smExit();
		}
		Hinted = envIsSyscallHinted(0x6F);
	}

	virtual void exitServices() override {
		CloseThreads();
		
		//Exit services
		svcCloseHandle(debug);
		dmntchtExit();
		clkrstExit();
		pcvExit();
		tsExit();
		tcExit();
		fanControllerClose(&g_ICon);
		fanExit();
		nvClose(fd);
		nvExit();
#ifdef CUSTOM
		psmExit();
#endif
	}

    virtual void onShow() override {}    // Called before overlay wants to change from invisible to visible state
    virtual void onHide() override {}    // Called before overlay wants to change from visible to invisible state

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<MainMenu>();  // Initial Gui to load. It's possible to pass arguments to it's constructor like this
    }
};

// This function gets called on startup to create a new Overlay object
int main(int argc, char **argv) {
    return tsl::loop<MonitorOverlay>(argc, argv);
}

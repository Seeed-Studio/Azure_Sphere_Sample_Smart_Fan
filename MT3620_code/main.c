#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "Grove/Grove.h"

#include "applibs_versions.h"
#include <applibs/log.h>
#include "mt3620_rdb.h"

#include "led_blink_utility.h"
#include "timer_utility.h"
#include <applibs/wificonfig.h>

#include "azure_iot_utilities.h"


// Change ssid and psk for your WiFi router 
const char *wifiSsid = "SSID";    
const char *wifiPsk = "PSK";

int uart3Fd = -1;
static const struct timespec LoopInterval = { 0, 200000000 };
static volatile sig_atomic_t TerminationRequired = false;

// An array defining the RGB GPIOs for each LED on the device
static const GPIO_Id ledsPins[2][3] = {
    {MT3620_RDB_LED1_RED, MT3620_RDB_LED1_GREEN, MT3620_RDB_LED1_BLUE}, {MT3620_RDB_NETWORKING_LED_RED, MT3620_RDB_NETWORKING_LED_GREEN, MT3620_RDB_NETWORKING_LED_BLUE}
};

// Connectivity state
static bool connectedToIoTHub = false;
static RgbLed ledMessageEventSentReceived = RGBLED_INIT_VALUE;
static RgbLed ledNetworkStatus = RGBLED_INIT_VALUE;
static RgbLed *rgbLeds[] = {&ledMessageEventSentReceived, &ledNetworkStatus};
static const size_t rgbLedsCount = sizeof(rgbLeds) / sizeof(*rgbLeds);

static void TerminationHandler(int signalNumber)
{
	// Don't use Log_Debug here, as it is not guaranteed to be async signal safe
	TerminationRequired = true;
}

static float TemperatureThresholdHigh = NAN;
static float TemperatureThresholdLow = NAN;

static void UARTHandler()
{
	const size_t receiveBufferSize = 64;
	uint8_t receiveBuffer[receiveBufferSize + 1]; // allow extra byte for string termination
	ssize_t bytesRead;

	// Read UART message
	bytesRead = read(uart3Fd, receiveBuffer, receiveBufferSize);
	if (bytesRead > 0) {
		Log_Debug(receiveBuffer);
		// Need Maintain
		if (NULL != (strstr(receiveBuffer, "Need Maintain")))
		{			
			AzureIoT_SendMessage("{\"body\":{\"message\":\"Fan Need Maintain\"}}");	
			Log_Debug("Fan Need Maintain.\n");
			// Set the send/receive LED to blink once immediately to indicate the message has been
			// queued
			LedBlinkUtility_BlinkNow(&ledMessageEventSentReceived, LedBlinkUtility_Colors_Red);
		}
	}
}

static void DeviceTwinUpdate(JSON_Object *desiredProperties)
{	
    LedBlinkUtility_BlinkNow(&ledMessageEventSentReceived, LedBlinkUtility_Colors_Yellow);

	JSON_Value *temperatureThresholdHighJson = json_object_get_value(desiredProperties, "temperatureThresholdHigh");
	if (temperatureThresholdHighJson != NULL && json_value_get_type(temperatureThresholdHighJson) == JSONNumber)
	{
		float temperatureThresholdHigh = (float)json_value_get_number(temperatureThresholdHighJson);
		Log_Debug("temperatureThresholdHigh = %.2f\n", temperatureThresholdHigh);
		AzureIoT_TwinReportState("temperatureThresholdHigh", (size_t)temperatureThresholdHigh);	// TODO Cannot report floating-point value.
		TemperatureThresholdHigh = temperatureThresholdHigh;
	}

	JSON_Value *temperatureThresholdLowJson = json_object_get_value(desiredProperties, "temperatureThresholdLow");
	if (temperatureThresholdLowJson != NULL && json_value_get_type(temperatureThresholdLowJson) == JSONNumber)
	{
		float temperatureThresholdLow = (float)json_value_get_number(temperatureThresholdLowJson);
		Log_Debug("temperatureThresholdLow = %.2f\n", temperatureThresholdLow);
		AzureIoT_TwinReportState("temperatureThresholdLow", (size_t)temperatureThresholdLow);	// TODO Cannot report floating-point value.
		TemperatureThresholdLow = temperatureThresholdLow;
	}
}

void Display_Temperature(void* inst, int temp)
{
	// Symbol of degree centigrade
	Grove4DigitDisplay_DisplayOneSegment(inst, 3, 0xc);
	// Temperature with one decimal
	Grove4DigitDisplay_DisplayOneSegment(inst, 2, temp % 10);
	temp /= 10;
	Grove4DigitDisplay_DisplayOneSegment(inst, 1, temp % 10);
	temp /= 10;
	Grove4DigitDisplay_DisplayOneSegment(inst, 0, temp % 10);
}

/// <summary>
///     IoT Hub connection status callback function.
/// </summary>
/// <param name="connected">'true' when the connection to the IoT Hub is established.</param>
static void IoTHubConnectionStatusChanged(bool connected)
{
	connectedToIoTHub = connected;	
}


/// <summary>
///     Store a sample WiFi network.
/// </summary>
static void AddSampleWiFiNetwork(void)
{
    int result = 0;    
    result = WifiConfig_StoreWpa2Network((uint8_t*)wifiSsid, strlen(wifiSsid),
                                          wifiPsk, strlen(wifiPsk));
    if (result < 0) {
        if (errno == EEXIST) {
            Log_Debug("INFO: The \"%s\" WiFi network is already stored on the device.\n", wifiSsid);
        } else {
            Log_Debug(
                "ERROR: WifiConfig_StoreOpenNetwork failed to store WiFi network \"%s\" with "
                "result %d. Errno: %s (%d).\n",
                wifiSsid, result, strerror(errno), errno);
        }
    } else {
        Log_Debug("INFO: Successfully stored WiFi network: \"%s\".\n", wifiSsid);
    }
}

/// <summary>
///     List all stored WiFi networks.
/// </summary>
static void DebugPrintStoredWiFiNetworks(void)
{
    int result = WifiConfig_GetStoredNetworkCount();
    if (result < 0) {
        Log_Debug(
            "ERROR: WifiConfig_GetStoredNetworkCount failed to get stored network count with "
            "result %d. Errno: %s (%d).\n",
            result, strerror(errno), errno);
    } else if (result == 0) {
        Log_Debug("INFO: There are no stored WiFi networks.\n");
    } else {
        Log_Debug("INFO: There are %d stored WiFi networks:\n", result);
        size_t networkCount = (size_t)result;
        WifiConfig_StoredNetwork *networks =
            (WifiConfig_StoredNetwork *)malloc(sizeof(WifiConfig_StoredNetwork) * networkCount);
        int result = WifiConfig_GetStoredNetworks(networks, networkCount);
        if (result < 0) {
            Log_Debug(
                "ERROR: WifiConfig_GetStoredNetworks failed to get stored networks with "
                "result %d. Errno: %s (%d).\n",
                result, strerror(errno), errno);
        } else {
            networkCount = (size_t)result;
            for (size_t i = 0; i < networkCount; ++i) {
                Log_Debug("INFO: %3d) SSID \"%.*s\"\n", i, networks[i].ssidLength,
                          networks[i].ssid);
            }
        }
        free(networks);
    }
}

/// <summary>
///     Show details of the currently connected WiFi network.
/// </summary>
static void DebugPrintCurrentlyConnectedWiFiNetwork(void)
{
    WifiConfig_ConnectedNetwork network;
    int result = WifiConfig_GetCurrentNetwork(&network);
    if (result < 0) {
        Log_Debug("INFO: Not currently connected to a WiFi network.\n");
    } else {
        Log_Debug("INFO: Currently connected WiFi network: ");
        Log_Debug("INFO: SSID \"%.*s\", BSSID %02x:%02x:%02x:%02x:%02x:%02x, Frequency %dMHz\n",
                  network.ssidLength, network.ssid, network.bssid[0], network.bssid[1],
                  network.bssid[2], network.bssid[3], network.bssid[4], network.bssid[5],
                  network.frequencyMHz);
    }
}

/// <summary>
///     Trigger a WiFi scan and list found WiFi networks.
/// </summary>
// static void DebugPrintScanFoundNetworks(void)
// {
//     int result = WifiConfig_TriggerScanAndGetScannedNetworkCount();
//     if (result < 0) {
//         Log_Debug(
//             "ERROR: WifiConfig_TriggerScanAndGetScannedNetworkCount failed to get scanned "
//             "network count with result %d. Errno: %s (%d).\n",
//             result, strerror(errno), errno);
//     } else if (result == 0) {
//         Log_Debug("INFO: Scan found no WiFi network.\n");
//     } else {
//         size_t networkCount = (size_t)result;
//         Log_Debug("INFO: Scan found %d WiFi networks:\n", result);
//         WifiConfig_ScannedNetwork *networks =
//             (WifiConfig_ScannedNetwork *)malloc(sizeof(WifiConfig_ScannedNetwork) * networkCount);
//         result = WifiConfig_GetScannedNetworks(networks, networkCount);
//         if (result < 0) {
//             Log_Debug(
//                 "ERROR: WifiConfig_GetScannedNetworks failed to get scanned networks with "
//                 "result %d. Errno: %s (%d).\n",
//                 result, strerror(errno), errno);
//         } else {
//             // Log SSID, signal strength and frequency of the found WiFi networks
//             networkCount = (size_t)result;
//             for (size_t i = 0; i < networkCount; ++i) {
//                 Log_Debug("INFO: %3d) SSID \"%.*s\", Signal Level %d, Frequency %dMHz\n", i,
//                           networks[i].ssidLength, networks[i].ssid, networks[i].signalRssi,
//                           networks[i].frequencyMHz);
//             }
//         }
//         free(networks);
//     }
// }


int main(int argc, char *argv[])
{
	Log_Debug("AzureSphereDemo1 application starting\n");

	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = TerminationHandler;
	sigaction(SIGTERM, &action, NULL);

	////////////////////////////////////////////////////////////////////////////////
	// Open file descriptors for the RGB LEDs and store them in the rgbLeds array (and in turn in
	// the ledMessageEventSentReceived, ledNetworkStatus variables)
    LedBlinkUtility_OpenLeds(rgbLeds, rgbLedsCount, ledsPins);

	 // Perform WiFi network setup and debug printing
    AddSampleWiFiNetwork();
    DebugPrintStoredWiFiNetworks();
    DebugPrintCurrentlyConnectedWiFiNetwork();
    // DebugPrintScanFoundNetworks();

	////////////////////////////////////////
	// Grove

	int i2cFd;	
	GroveShield_Initialize(&i2cFd);

	uart3Fd = GroveUART_Open(MT3620_RDB_HEADER3_ISU3_UART, 9600);

	void* relay = GroveRelay_Open(0);
	void* display = Grove4DigitDisplay_Open(4, 5);	
	void* sht31 = GroveTempHumiSHT31_Open(i2cFd);
	void* ad7992 = GroveAD7992_Open(i2cFd, 58, 57);

	// Display clock point for Grove 4 Digit Display
	Grove4DigitDisplay_DisplayClockPoint(true);

	////////////////////////////////////////
	// AzureIoT

	if (!AzureIoT_Initialize())
	{
		Log_Debug("ERROR: Cannot initialize Azure IoT Hub SDK.\n");
		return -1;
	}
	AzureIoT_SetDeviceTwinUpdateCallback(&DeviceTwinUpdate);
	AzureIoT_SetConnectionStatusCallback(&IoTHubConnectionStatusChanged);
	AzureIoT_SetupClient();

	////////////////////////////////////////
	// Loop

	bool fanSupplyOn = false;
	GroveRelay_Off(relay);

	static uint8_t fan_pwr_level = 1;

	while (!TerminationRequired)
	{
		// Set network status LED color
        LedBlinkUtility_Colors color =
            (connectedToIoTHub ? LedBlinkUtility_Colors_Green : LedBlinkUtility_Colors_Off);
        if (LedBlinkUtility_SetLed(&ledNetworkStatus, color) != 0) {
            Log_Debug("ERROR: Set color for network status LED failed\n");
            break;
        }
		
		UARTHandler();

		AzureIoT_DoPeriodicTasks();

		if (fanSupplyOn)
		{
			char fanCtrlCmd[16];
			sprintf(fanCtrlCmd, "pwr_level:%d\r\n", fan_pwr_level);
			// sprintf(fanCtrlCmd, "fan_pwr_level:%d\r\n", 1);
			Log_Debug(fanCtrlCmd);
			GroveUART_Write(uart3Fd, fanCtrlCmd, (int)strlen(fanCtrlCmd));			
		}

		GroveTempHumiSHT31_Read(sht31);
		float temp = GroveTempHumiSHT31_GetTemperature(sht31);
		Log_Debug("Temperature: %.1fC\n", temp);

		Display_Temperature(display, (int)(temp * 10));		

		if (!fanSupplyOn && TemperatureThresholdHigh != NAN && temp >= TemperatureThresholdHigh)
		{
			fanSupplyOn = true;
			GroveRelay_On(relay);			
			Log_Debug("ON the fan.\n");			
		}
		else if (fanSupplyOn && TemperatureThresholdLow != NAN && temp <= TemperatureThresholdLow) 
		{
			fanSupplyOn = false;
			GroveRelay_Off(relay);			
			Log_Debug("OFF the fan.\n");
		}

		float volume = 1.0f - GroveAD7992_Read(ad7992, 0);
		// Log_Debug("ADC value: %f\r\n", volume);
		 if (volume <= 0.05f)
		 {			
		 	fan_pwr_level = 0;
		 }
		 else if (volume > 0.05f && volume <= 0.40f)
		 {
		 	fan_pwr_level = 1;
		 }
		 else if (volume > 0.40f && volume <= 0.75f)
		 {
		 	fan_pwr_level = 2;
		 }
		 else if (volume > 0.75f && volume <= 1.0f)
		 {			
		 	fan_pwr_level = 3;
		 }

		nanosleep(&LoopInterval, NULL);
	}

	////////////////////////////////////////
	// Terminate

	Log_Debug("Application exiting\n");
	return 0;
}

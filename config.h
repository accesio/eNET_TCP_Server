#pragma once
/*
config.h

eNET-AIO configuration structure and support API.

	All configuration that should be stored non-volatile is held in TConfig structures.
	The "currently in-use or about to be" configuration is TConfig Config; global.

	There are Factory, Current, and User configurations in nonvolatile storage on eNET-AIO.

	The different configuration types are stored in directories per type under /etc/opt/aioenet/,
	config.factory/, config.current/, and config.user/

	NOTE: Future revisions may support multiple user configs

	aioenetd (or other users of this config.h/.cpp) should:
		call InitConfig(Config) to set "useful" defaults in the global Config struct.
		call InitializeConfigFiles() to create the config directories and files if they are not found
		call LoadConfig(CONFIG_FACTORY), which will update Config with any found .conf values
		call LoadConfig(CONFIG_CURRENT), which will update the Config with the customer's desired values
*/

#include <string>

#include "utilities.h"
#include "eNET-AIO16-16F.h"

#define CONFIG_PATH "/var/lib/aioenetd/"

#define CONFIG_CURRENT "config.current/"
#define CONFIG_FACTORY "config.factory/"
#define CONFIG_USER    "config.user/"
#define CONFIG_INIT_STAMP ".initialized"
#define AIOENETD_VERSION "aioenetd " __DATE__ " " __TIME__


/*
RangeCode 0 = 0-10 V
RangeCode 1 = ±10 V
RangeCode 2 = 0-5 V
RangeCode 3 = ±5 V
RangeCode 4 = 0-2 V
RangeCode 5 = ±2 V
RangeCode 6 = 0-1 V
RangeCode 7 = ±1 V
*/
#pragma pack(push,1)
using TConfig = struct TConfigStruct {
	std::string Hostname;
	std::string Model;
	std::string Description;
	std::string SerialNumber;
	__u32 FpgaVersionCode; 							// read off the hardware
	__u8 features; 									// read off the hardware
	__u8 numberOfSubmuxes;							// set by TBRD_NumberOfSubmuxes
	std::string submuxBarcodes[maxSubmuxes];
	std::string submuxTypes[maxSubmuxes];
	float submuxScaleFactors[maxSubmuxes][gainGroupsPerSubmux]; // set by TBRD_SubmuxScale
	float submuxOffsets[maxSubmuxes][gainGroupsPerSubmux];      // set by TBRD_SubmuxOffset
	__u8 adcDifferential; // bit map bit0=adcCh0, set == differential // J2H: TODO: may want to invert this; may want 16 of them, may want 128 of them, may want 16+128 of them
	__u32 adcRangeCodes[16];
	float adcScaleCoefficients[8];
	float adcOffsetCoefficients[8];
	__u8 NUM_DACs;
	__u32 dacRanges[MAX_DACs];
	float dacScaleCoefficients[MAX_DACs];
	float dacOffsetCoefficients[MAX_DACs];
};
#pragma pack(pop)

extern TConfig Config;

void InitConfig(TConfig &config);

void LoadBrdConfig(std::string which = CONFIG_CURRENT);
void LoadSubmuxConfig(std::string which = CONFIG_CURRENT);
void LoadAdcConfig(std::string which = CONFIG_CURRENT);
void LoadDacConfig(std::string which = CONFIG_CURRENT);
void LoadAdcCalConfig(std::string which = CONFIG_CURRENT);
void LoadDacCalConfig(std::string which = CONFIG_CURRENT);
void LoadCalConfig(std::string which = CONFIG_CURRENT);
void LoadConfig(std::string which = CONFIG_CURRENT);

bool SaveBrdConfig(std::string which = CONFIG_CURRENT);
bool SaveDacCalConfig(std::string which = CONFIG_CURRENT);
bool SaveAdcCalConfig(std::string which = CONFIG_CURRENT);
bool SaveCalConfig(std::string which = CONFIG_CURRENT);
bool SaveAdcConfig(std::string which = CONFIG_CURRENT);
bool SaveSubmuxConfig(std::string which = CONFIG_CURRENT);
bool SaveConfig(std::string which = CONFIG_CURRENT);
int WriteConfigString(std::string key, std::string value, std::string which = CONFIG_CURRENT);

void ApplyConfig();


// create /var/aioenetd/ if missing.
// create /var/aioenetd/config.factory/ if missing.
// create /var/aioenetd/config.current/ if missing.
// create /var/aioenetd/config.user/ if missing.
// create individual /var/aioenetd/config.factory/foo.conf files for all config. fields that do not already have foo.conf.
// if any individual foo.conf were created copy it to config.current/foo.conf if missing there.
// if any individual foo.conf were created copy it to config.user/foo.conf if missing there.
void InitializeConfigFiles(TConfig &config); // should only run if /var/aioenetd/config.factory/ is missing or empty

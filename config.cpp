//------------------- Configuration Files -------------
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

#include "utilities.h"
#include "logging.h"
#include "apci.h"
#include "config.h"

#define filename (CONFIG_PATH + which + key + ".conf")
TConfig Config;


/* There are 16 ADC ranges, one per base board channel
   There can be one submux (AIMUX64M) attached, which turns each ADC channel of the base board into 4 more channels (64 total)
   or up to four submux (AIMUX32), which turn groups of four ADC channels into 32 channels. With the AIMUX32 each ADC channel's group
   of 8 submux channels can have complex signal conditioning features applied.  The AIMUX64M merely allows an additional gain factor.
   This Config struct allows per-submux-channel-group calibration and scaling, as well as the range and calibration for the four DACs and ADC.
*/
void InitConfig(TConfig &config) {
	config.Hostname = "enetaio000000000000000";
	config.Model = "eNET-untested";
	config.Description = "eNET-untested Description";
	config.SerialNumber = "eNET-unset Serial Number";
	config.FpgaVersionCode = 0xDEADBA57;
	config.numberOfSubmuxes = 0;
	config.adcDifferential = 0b00000000;
	for (int i = 0; i < 16; i++) {
		if (i < 4) {
			config.submuxBarcodes[i]="";
			config.submuxTypes[i]="";
			for(int submux=0; submux<4; submux++) {
				config.submuxScaleFactors[i][submux] = 1.0;
				config.submuxOffsets[i][submux] = 0.0;
			}
			config.dacRanges[i] = 1;
			config.dacScaleCoefficients[i] = 1.00000000;
			config.dacOffsetCoefficients[i] = 0.0;
		}
		if (i < 8) {
			config.adcScaleCoefficients[i] = 1.0;
			config.adcOffsetCoefficients[i] = 0.0;

		}
		if (i < 16) {
			config.adcRangeCodes[i]=1;
		}
	}
	std::cout << "Config.dacScaleCoefficients[0] configured with "<< Config.dacScaleCoefficients[0]<<" during InitConfig"<<'\n';
}

int ReadConfigString(std::string key, std::string &value, std::string which )
{
	auto f = open(filename.c_str(), O_RDONLY | O_CLOEXEC | O_SYNC);
	int bytesRead = -1;
	if (f < 0)
	{
		bytesRead = -errno;
		Error("failed to open() on " + filename + ", code " + std::to_string(bytesRead));
		perror("ReadConfigString() file open failed ");
		return bytesRead;
	}
	__u8 buf[257];
	bytesRead = read(f, buf, 256);
	if (bytesRead < 0)
	{
		bytesRead = errno;
		Error("ReadConfigString() " + filename + " failed, bytesRead=" + std::to_string(bytesRead) + ", status " + std::to_string(bytesRead));
		perror("ReadConfigString() failed ");
		return bytesRead;
	}

	buf[bytesRead] = 0;

	value = std::string((char *)buf);
	// std::cout << '\n'
			//   << "ReadConfigString(" << key << ") got " << value << '\n'
			//   << '\n';
	return bytesRead;
}

int ReadConfigU8(std::string key, __u8 &value, std::string which )
{
	std::string v;
	int bytesRead = ReadConfigString(key, v);
	if (bytesRead == 2){
		// std::cout << "ReadConfigU8 got " << v << " bytes read == " << bytesRead << '\n'<< '\n';

		value = std::stoi(v, nullptr, 16);
		}
	return bytesRead;
}

int ReadConfigU32(std::string key, __u32 &value, std::string which )
{
	std::string v;
	int bytesRead = ReadConfigString(key, v);
	if (bytesRead == 8)
		value = std::stoi(v, nullptr, 16);
	return bytesRead;
}

int ReadConfigFloat(std::string key, float &value, std::string which )
{
	__u32 v;
	int bytesRead = ReadConfigU32(key, v);

	// if (bytesRead == 8){
	// 	map_f__u32.u = v;
	// 	value = map_f__u32.f;
	// }
	// std::cout << "ReadConfigFloat(" << key << ") got " << value << " (from "<< std::hex << v <<") with status " << bytesRead <<'\n';
	return bytesRead;
}



int WriteConfigString(std::string key, std::string value, std::string which ){
	// validate key, value, and filename are valid/safe to use
	// if file == "config.current" then key must already exist
	// ConfigWrite(file, key, value);
	//std::string filename = CONFIG_PATH + file + "/" + key + ".conf";
	int bytesRead = -1;
	auto f = open(filename.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC | O_SYNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	if (f < 0)
	{
		bytesRead = errno;
		Error("failed open() on " + filename + " code: " + std::to_string(bytesRead));
		perror("open() failed");
	}
	else
	{
		bytesRead = write(f, value.c_str(), strlen(value.c_str()));
		if (bytesRead < 0)
		{
			bytesRead = -errno;
			Error("failed open() on " + filename + " code: " + std::to_string(bytesRead));
			perror("WriteConfigString() open() failed");
		}	close(f);
	}
	// Trace(which + key + " = " + value );
	return bytesRead;
}

int WriteConfigU8(std::string key, __u8 value, std::string which )
{
	std::string v = to_hex<__u8>(value);
	return WriteConfigString(key, v, which);
}

int WriteConfigU32(std::string key, __u32 value, std::string which )
{
	std::string v = to_hex<__u32>(value);
	return WriteConfigString(key, v, which);
}

int WriteConfigFloat(std::string key, float value, std::string which )
{
	//map_f__u32.f = value;
	__u32 v = bit_cast<__u32>(value);
	return WriteConfigU32(key, v, which);
	// std::cout << "	 wrote "+key+" as with "<< value <<" during WriteConfigFloat"<<'\n';
}

// TODO: improve error handling program-wide
#define HandleError(x) {if (x<0)	\
		Error("Error");				\
}


/* LOAD CONFIGURATION STRUCT FROM DISK FILES */
void LoadDacCalConfig(std::string which)
{
	HandleError(ReadConfigFloat("DAC_ScaleCh0", Config.dacScaleCoefficients[0], which));
	HandleError(ReadConfigFloat("DAC_ScaleCh1", Config.dacScaleCoefficients[1], which));
	HandleError(ReadConfigFloat("DAC_ScaleCh2", Config.dacScaleCoefficients[2], which));
	HandleError(ReadConfigFloat("DAC_ScaleCh3", Config.dacScaleCoefficients[3], which));

	HandleError(ReadConfigFloat("DAC_OffsetCh0", Config.dacOffsetCoefficients[0], which));
	HandleError(ReadConfigFloat("DAC_OffsetCh1", Config.dacOffsetCoefficients[1], which));
	HandleError(ReadConfigFloat("DAC_OffsetCh2", Config.dacOffsetCoefficients[2], which));
	HandleError(ReadConfigFloat("DAC_OffsetCh3", Config.dacOffsetCoefficients[3], which));
}
void LoadAdcCalConfig(std::string which)
{
	HandleError(ReadConfigFloat("ADC_ScaleRange0", Config.adcScaleCoefficients[0], which));
	HandleError(ReadConfigFloat("ADC_ScaleRange1", Config.adcScaleCoefficients[1], which));
	HandleError(ReadConfigFloat("ADC_ScaleRange2", Config.adcScaleCoefficients[2], which));
	HandleError(ReadConfigFloat("ADC_ScaleRange3", Config.adcScaleCoefficients[3], which));
	HandleError(ReadConfigFloat("ADC_ScaleRange4", Config.adcScaleCoefficients[4], which));
	HandleError(ReadConfigFloat("ADC_ScaleRange5", Config.adcScaleCoefficients[5], which));
	HandleError(ReadConfigFloat("ADC_ScaleRange6", Config.adcScaleCoefficients[6], which));
	HandleError(ReadConfigFloat("ADC_ScaleRange7", Config.adcScaleCoefficients[7], which));

	HandleError(ReadConfigFloat("ADC_OffsetRange0", Config.adcOffsetCoefficients[0], which));
	HandleError(ReadConfigFloat("ADC_OffsetRange1", Config.adcOffsetCoefficients[1], which));
	HandleError(ReadConfigFloat("ADC_OffsetRange2", Config.adcOffsetCoefficients[2], which));
	HandleError(ReadConfigFloat("ADC_OffsetRange3", Config.adcOffsetCoefficients[3], which));
	HandleError(ReadConfigFloat("ADC_OffsetRange4", Config.adcOffsetCoefficients[4], which));
	HandleError(ReadConfigFloat("ADC_OffsetRange5", Config.adcOffsetCoefficients[5], which));
	HandleError(ReadConfigFloat("ADC_OffsetRange6", Config.adcOffsetCoefficients[6], which));
	HandleError(ReadConfigFloat("ADC_OffsetRange7", Config.adcOffsetCoefficients[7], which));
}

void LoadCalConfig(std::string which)
{
	LoadDacCalConfig(which);
	LoadAdcCalConfig(which);
}

void LoadDacConfig(std::string which)
{	HandleError(ReadConfigU32("DAC_RangeCh0", Config.dacRanges[0], which));
	HandleError(ReadConfigU32("DAC_RangeCh1", Config.dacRanges[1], which));
	HandleError(ReadConfigU32("DAC_RangeCh2", Config.dacRanges[2], which));
	HandleError(ReadConfigU32("DAC_RangeCh3", Config.dacRanges[3], which));
}

void LoadAdcConfig(std::string which)
{
	HandleError(ReadConfigU8("ADC_Differential", Config.adcDifferential, which));

	HandleError(ReadConfigU32("ADC_RangeCodeCh00", Config.adcRangeCodes[0], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh01", Config.adcRangeCodes[1], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh02", Config.adcRangeCodes[2], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh03", Config.adcRangeCodes[3], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh04", Config.adcRangeCodes[4], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh05", Config.adcRangeCodes[5], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh06", Config.adcRangeCodes[6], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh07", Config.adcRangeCodes[7], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh08", Config.adcRangeCodes[8], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh09", Config.adcRangeCodes[9], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh10", Config.adcRangeCodes[10], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh11", Config.adcRangeCodes[11], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh12", Config.adcRangeCodes[12], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh13", Config.adcRangeCodes[13], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh14", Config.adcRangeCodes[14], which));
	HandleError(ReadConfigU32("ADC_RangeCodeCh15", Config.adcRangeCodes[15], which));
}

void LoadSubmuxConfig(std::string which)
{
	HandleError(ReadConfigU8("BRD_NumberOfSubmuxes", Config.numberOfSubmuxes, which));

	HandleError(ReadConfigString("BRD_SubmuxBarcode0", Config.submuxBarcodes[0], which));
	HandleError(ReadConfigString("BRD_SubmuxBarcode1", Config.submuxBarcodes[1], which));
	HandleError(ReadConfigString("BRD_SubmuxBarcode2", Config.submuxBarcodes[2], which));
	HandleError(ReadConfigString("BRD_SubmuxBarcode3", Config.submuxBarcodes[3], which));

	HandleError(ReadConfigString("BRD_SubmuxType0", Config.submuxTypes[0], which));
	HandleError(ReadConfigString("BRD_SubmuxType1", Config.submuxTypes[1], which));
	HandleError(ReadConfigString("BRD_SubmuxType2", Config.submuxTypes[2], which));
	HandleError(ReadConfigString("BRD_SubmuxType3", Config.submuxTypes[3], which));

	HandleError(ReadConfigFloat("BRD_Submux0Range0", Config.submuxScaleFactors[0][0], which));
	HandleError(ReadConfigFloat("BRD_Submux0Range1", Config.submuxScaleFactors[0][1], which));
	HandleError(ReadConfigFloat("BRD_Submux0Range2", Config.submuxScaleFactors[0][2], which));
	HandleError(ReadConfigFloat("BRD_Submux0Range3", Config.submuxScaleFactors[0][3], which));
	HandleError(ReadConfigFloat("BRD_Submux1Range0", Config.submuxScaleFactors[1][0], which));
	HandleError(ReadConfigFloat("BRD_Submux1Range1", Config.submuxScaleFactors[1][1], which));
	HandleError(ReadConfigFloat("BRD_Submux1Range2", Config.submuxScaleFactors[1][2], which));
	HandleError(ReadConfigFloat("BRD_Submux1Range3", Config.submuxScaleFactors[1][3], which));
	HandleError(ReadConfigFloat("BRD_Submux2Range0", Config.submuxScaleFactors[2][0], which));
	HandleError(ReadConfigFloat("BRD_Submux2Range1", Config.submuxScaleFactors[2][1], which));
	HandleError(ReadConfigFloat("BRD_Submux2Range2", Config.submuxScaleFactors[2][2], which));
	HandleError(ReadConfigFloat("BRD_Submux2Range3", Config.submuxScaleFactors[2][3], which));
	HandleError(ReadConfigFloat("BRD_Submux3Range0", Config.submuxScaleFactors[3][0], which));
	HandleError(ReadConfigFloat("BRD_Submux3Range1", Config.submuxScaleFactors[3][1], which));
	HandleError(ReadConfigFloat("BRD_Submux3Range2", Config.submuxScaleFactors[3][2], which));
	HandleError(ReadConfigFloat("BRD_Submux3Range3", Config.submuxScaleFactors[3][3], which));

	HandleError(ReadConfigFloat("BRD_Submux0Offset0", Config.submuxOffsets[0][0], which));
	HandleError(ReadConfigFloat("BRD_Submux0Offset1", Config.submuxOffsets[0][1], which));
	HandleError(ReadConfigFloat("BRD_Submux0Offset2", Config.submuxOffsets[0][2], which));
	HandleError(ReadConfigFloat("BRD_Submux0Offset3", Config.submuxOffsets[0][3], which));
	HandleError(ReadConfigFloat("BRD_Submux1Offset0", Config.submuxOffsets[1][0], which));
	HandleError(ReadConfigFloat("BRD_Submux1Offset1", Config.submuxOffsets[1][1], which));
	HandleError(ReadConfigFloat("BRD_Submux1Offset2", Config.submuxOffsets[1][2], which));
	HandleError(ReadConfigFloat("BRD_Submux1Offset3", Config.submuxOffsets[1][3], which));
	HandleError(ReadConfigFloat("BRD_Submux2Offset0", Config.submuxOffsets[2][0], which));
	HandleError(ReadConfigFloat("BRD_Submux2Offset1", Config.submuxOffsets[2][1], which));
	HandleError(ReadConfigFloat("BRD_Submux2Offset2", Config.submuxOffsets[2][2], which));
	HandleError(ReadConfigFloat("BRD_Submux2Offset3", Config.submuxOffsets[2][3], which));
	HandleError(ReadConfigFloat("BRD_Submux3Offset0", Config.submuxOffsets[3][0], which));
	HandleError(ReadConfigFloat("BRD_Submux3Offset1", Config.submuxOffsets[3][1], which));
	HandleError(ReadConfigFloat("BRD_Submux3Offset2", Config.submuxOffsets[3][2], which));
	HandleError(ReadConfigFloat("BRD_Submux3Offset3", Config.submuxOffsets[3][3], which));
}

// Reads the /etc/aioenetd.d/config.current/configuration data into the Config structure
void LoadConfig(std::string which)
{
	{
	// read /etc/hostname into Config.Hostname
		std::ifstream in("/etc/hostname");
		in >> Config.Hostname;
		in.close();
		Debug("Hostname == " + Config.Hostname);
	}

	HandleError(ReadConfigString("BRD_Model", Config.Model, which));
	HandleError(ReadConfigString("BRD_Description", Config.Description, which));
	HandleError(ReadConfigString("BRD_SerialNumber", Config.SerialNumber, which));

	LoadCalConfig(which);
	LoadDacConfig(which);
	LoadAdcConfig(which);
	LoadSubmuxConfig(which);
}





/* SAVE CONFIGURATION STRUCT TO DISK */
bool SaveDacCalConfig(std::string which)
{
	HandleError(WriteConfigFloat("DAC_ScaleCh0", Config.dacScaleCoefficients[0], which));
	HandleError(WriteConfigFloat("DAC_ScaleCh1", Config.dacScaleCoefficients[1], which));
	HandleError(WriteConfigFloat("DAC_ScaleCh2", Config.dacScaleCoefficients[2], which));
	HandleError(WriteConfigFloat("DAC_ScaleCh3", Config.dacScaleCoefficients[3], which));

	HandleError(WriteConfigFloat("DAC_OffsetCh0", Config.dacOffsetCoefficients[0], which));
	HandleError(WriteConfigFloat("DAC_OffsetCh1", Config.dacOffsetCoefficients[1], which));
	HandleError(WriteConfigFloat("DAC_OffsetCh2", Config.dacOffsetCoefficients[2], which));
	HandleError(WriteConfigFloat("DAC_OffsetCh3", Config.dacOffsetCoefficients[3], which));
	return true;
}

bool SaveAdcCalConfig(std::string which)
{
	HandleError(WriteConfigFloat("ADC_ScaleRange0", Config.adcScaleCoefficients[0], which));
	HandleError(WriteConfigFloat("ADC_ScaleRange1", Config.adcScaleCoefficients[1], which));
	HandleError(WriteConfigFloat("ADC_ScaleRange2", Config.adcScaleCoefficients[2], which));
	HandleError(WriteConfigFloat("ADC_ScaleRange3", Config.adcScaleCoefficients[3], which));
	HandleError(WriteConfigFloat("ADC_ScaleRange4", Config.adcScaleCoefficients[4], which));
	HandleError(WriteConfigFloat("ADC_ScaleRange5", Config.adcScaleCoefficients[5], which));
	HandleError(WriteConfigFloat("ADC_ScaleRange6", Config.adcScaleCoefficients[6], which));
	HandleError(WriteConfigFloat("ADC_ScaleRange7", Config.adcScaleCoefficients[7], which));

	HandleError(WriteConfigFloat("ADC_OffsetRange0", Config.adcOffsetCoefficients[0], which));
	HandleError(WriteConfigFloat("ADC_OffsetRange1", Config.adcOffsetCoefficients[1], which));
	HandleError(WriteConfigFloat("ADC_OffsetRange2", Config.adcOffsetCoefficients[2], which));
	HandleError(WriteConfigFloat("ADC_OffsetRange3", Config.adcOffsetCoefficients[3], which));
	HandleError(WriteConfigFloat("ADC_OffsetRange4", Config.adcOffsetCoefficients[4], which));
	HandleError(WriteConfigFloat("ADC_OffsetRange5", Config.adcOffsetCoefficients[5], which));
	HandleError(WriteConfigFloat("ADC_OffsetRange6", Config.adcOffsetCoefficients[6], which));
	HandleError(WriteConfigFloat("ADC_OffsetRange7", Config.adcOffsetCoefficients[7], which));
	return true;
}

bool SaveCalConfig(std::string which)
{
	SaveDacCalConfig(which);
	SaveAdcCalConfig(which);
	return true;
}

bool SaveDacConfig(std::string which)
{
	HandleError(WriteConfigU32("DAC_RangeCh0", Config.dacRanges[0], which));
	HandleError(WriteConfigU32("DAC_RangeCh1", Config.dacRanges[1], which));
	HandleError(WriteConfigU32("DAC_RangeCh2", Config.dacRanges[2], which));
	HandleError(WriteConfigU32("DAC_RangeCh3", Config.dacRanges[3], which));
	return true;
}

bool SaveAdcConfig(std::string which)
{
	HandleError(WriteConfigU8("ADC_Differential", Config.adcDifferential, which));

	HandleError(WriteConfigU32("ADC_RangeCodeCh00", Config.adcRangeCodes[0], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh01", Config.adcRangeCodes[1], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh02", Config.adcRangeCodes[2], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh03", Config.adcRangeCodes[3], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh04", Config.adcRangeCodes[4], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh05", Config.adcRangeCodes[5], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh06", Config.adcRangeCodes[6], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh07", Config.adcRangeCodes[7], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh08", Config.adcRangeCodes[8], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh09", Config.adcRangeCodes[9], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh10", Config.adcRangeCodes[10], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh11", Config.adcRangeCodes[11], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh12", Config.adcRangeCodes[12], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh13", Config.adcRangeCodes[13], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh14", Config.adcRangeCodes[14], which));
	HandleError(WriteConfigU32("ADC_RangeCodeCh15", Config.adcRangeCodes[15], which));
	return true;
}

bool SaveSubmuxConfig(std::string which)
{
	HandleError(WriteConfigU8("BRD_NumberOfSubmuxes", Config.numberOfSubmuxes, which));

	HandleError(WriteConfigString("BRD_SubmuxBarcode0", Config.submuxBarcodes[0], which));
	HandleError(WriteConfigString("BRD_SubmuxBarcode1", Config.submuxBarcodes[1], which));
	HandleError(WriteConfigString("BRD_SubmuxBarcode2", Config.submuxBarcodes[2], which));
	HandleError(WriteConfigString("BRD_SubmuxBarcode3", Config.submuxBarcodes[3], which));

	HandleError(WriteConfigString("BRD_SubmuxType0", Config.submuxTypes[0], which));
	HandleError(WriteConfigString("BRD_SubmuxType1", Config.submuxTypes[1], which));
	HandleError(WriteConfigString("BRD_SubmuxType2", Config.submuxTypes[2], which));
	HandleError(WriteConfigString("BRD_SubmuxType3", Config.submuxTypes[3], which));

	HandleError(WriteConfigFloat("BRD_Submux0Range0", Config.submuxScaleFactors[0][0], which));
	HandleError(WriteConfigFloat("BRD_Submux0Range1", Config.submuxScaleFactors[0][1], which));
	HandleError(WriteConfigFloat("BRD_Submux0Range2", Config.submuxScaleFactors[0][2], which));
	HandleError(WriteConfigFloat("BRD_Submux0Range3", Config.submuxScaleFactors[0][3], which));
	HandleError(WriteConfigFloat("BRD_Submux1Range0", Config.submuxScaleFactors[1][0], which));
	HandleError(WriteConfigFloat("BRD_Submux1Range1", Config.submuxScaleFactors[1][1], which));
	HandleError(WriteConfigFloat("BRD_Submux1Range2", Config.submuxScaleFactors[1][2], which));
	HandleError(WriteConfigFloat("BRD_Submux1Range3", Config.submuxScaleFactors[1][3], which));
	HandleError(WriteConfigFloat("BRD_Submux2Range0", Config.submuxScaleFactors[2][0], which));
	HandleError(WriteConfigFloat("BRD_Submux2Range1", Config.submuxScaleFactors[2][1], which));
	HandleError(WriteConfigFloat("BRD_Submux2Range2", Config.submuxScaleFactors[2][2], which));
	HandleError(WriteConfigFloat("BRD_Submux2Range3", Config.submuxScaleFactors[2][3], which));
	HandleError(WriteConfigFloat("BRD_Submux3Range0", Config.submuxScaleFactors[3][0], which));
	HandleError(WriteConfigFloat("BRD_Submux3Range1", Config.submuxScaleFactors[3][1], which));
	HandleError(WriteConfigFloat("BRD_Submux3Range2", Config.submuxScaleFactors[3][2], which));
	HandleError(WriteConfigFloat("BRD_Submux3Range3", Config.submuxScaleFactors[3][3], which));

	HandleError(WriteConfigFloat("BRD_Submux0Offset0", Config.submuxOffsets[0][0], which));
	HandleError(WriteConfigFloat("BRD_Submux0Offset1", Config.submuxOffsets[0][1], which));
	HandleError(WriteConfigFloat("BRD_Submux0Offset2", Config.submuxOffsets[0][2], which));
	HandleError(WriteConfigFloat("BRD_Submux0Offset3", Config.submuxOffsets[0][3], which));
	HandleError(WriteConfigFloat("BRD_Submux1Offset0", Config.submuxOffsets[1][0], which));
	HandleError(WriteConfigFloat("BRD_Submux1Offset1", Config.submuxOffsets[1][1], which));
	HandleError(WriteConfigFloat("BRD_Submux1Offset2", Config.submuxOffsets[1][2], which));
	HandleError(WriteConfigFloat("BRD_Submux1Offset3", Config.submuxOffsets[1][3], which));
	HandleError(WriteConfigFloat("BRD_Submux2Offset0", Config.submuxOffsets[2][0], which));
	HandleError(WriteConfigFloat("BRD_Submux2Offset1", Config.submuxOffsets[2][1], which));
	HandleError(WriteConfigFloat("BRD_Submux2Offset2", Config.submuxOffsets[2][2], which));
	HandleError(WriteConfigFloat("BRD_Submux2Offset3", Config.submuxOffsets[2][3], which));
	HandleError(WriteConfigFloat("BRD_Submux3Offset0", Config.submuxOffsets[3][0], which));
	HandleError(WriteConfigFloat("BRD_Submux3Offset1", Config.submuxOffsets[3][1], which));
	HandleError(WriteConfigFloat("BRD_Submux3Offset2", Config.submuxOffsets[3][2], which));
	HandleError(WriteConfigFloat("BRD_Submux3Offset3", Config.submuxOffsets[3][3], which));
	return true;
}

// saves the active configuration into current.config or which
bool SaveConfig(std::string which)
{
	Debug("SaveConfig writing Config struct to config files " + which);
	HandleError(WriteConfigString("BRD_Description", Config.Description, which));
	HandleError(WriteConfigString("BRD_Model", Config.Model, which));
	HandleError(WriteConfigString("BRD_SerialNumber", Config.SerialNumber, which));

	SaveCalConfig(which);
	SaveDacConfig(which);
	SaveAdcConfig(which);
	SaveSubmuxConfig(which);

	return true;
}

void ApplyAdcCalConfig()
{
	for (int cal=0;cal<8;++cal){
		out(ofsAdcCalScale + cal*ofsAdcCalScaleStride, *reinterpret_cast<__u32 *>(&Config.adcScaleCoefficients[cal]));
		out(ofsAdcCalOffset + cal*ofsAdcCalOffsetStride, *reinterpret_cast<__u32 *>(&Config.adcOffsetCoefficients[cal]));
	}
}

void ApplyConfig()
{
	ApplyAdcCalConfig();
}
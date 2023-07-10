
std::string TDAC_Range1::AsString(bool bAsReply)
{
	return "DAC_Range1("+std::to_string(this->dacChannel)+",0x"+to_hex<__u32>(this->dacRange)+")";
};
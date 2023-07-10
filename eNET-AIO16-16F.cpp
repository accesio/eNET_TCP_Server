
// crap function returns 8 or 32 for valid offsets into eNET-AIO's register map, or 0 for invalid
// specific to eNET-AIO register map
int widthFromOffset(int ofs)
{
	// LOG_IT;
	if (ofs < 0x18)
		return 8;
	else if ((ofs <= 0xFC) && (ofs % 4 == 0))
		return 32;
	return 0;
}

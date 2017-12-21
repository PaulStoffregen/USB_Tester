#include <Arduino.h>
#include <SD.h>

static File hexfile;
static int bufpos=0;
static int buflen=0;
static char buffer[512];
static char linebuf[120];
static uint32_t extended_addr;
static bool end_of_data;

static int getline(File f, char *line, int maxlen)
{
	int i=0;

	while (1) {
		while (bufpos < buflen) {
			char c = buffer[bufpos++];
			if (c == 0) {
				line[i] = 0;
				return i;
			} else if (c == 10 || c == 13) {
				if (i > 0) {
					line[i] = 0;
					return i;
				}
			} else {
				line[i++] = c;
				if (i >= maxlen - 1) {
					line[i] = 0;
					return i;
				}
			}
		}
		int n = f.read(buffer, 512);
		//Serial.printf("  read %d\n", n);
		if (n <= 0) {
			line[i] = 0;
			return i;
		}
		buflen = n;
		bufpos = 0;
	}
}

static uint32_t hex(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return 16;
}


static bool hex2(const char *p, uint32_t *num)
{
	uint32_t n1, n2;
	if ((n1 = hex(p[0])) >= 16) return false;
	if ((n2 = hex(p[1])) >= 16) return false;
	*num = (n1 << 4) | n2;
	return true;
}

static bool hex4(const char *p, uint32_t *num)
{
	uint32_t n1, n2, n3, n4;
	if ((n1 = hex(p[0])) >= 16) return false;
	if ((n2 = hex(p[1])) >= 16) return false;
	if ((n3 = hex(p[2])) >= 16) return false;
	if ((n4 = hex(p[3])) >= 16) return false;
	*num = (n1 << 12) | (n2 << 8) | (n3 << 4) | n4;
	return true;
}


static bool parse_hex(const char *line, uint32_t *addr, uint8_t *data,
	uint32_t maxlen, uint32_t *len)
{
	uint32_t code, sum, cksum, extaddr;

	if (line[0] != ':') return false;
	if (!hex2(line + 1, len)) return false;
	if (!hex4(line + 3, addr)) return false;
	if (!hex2(line + 7, &code)) return false;
	sum = *len + (*addr >> 8) + (*addr & 255) + code;
	if (code == 1) {
		// end of data line
		*len = 0;
		*addr = 0xFFFFFFFF;
		//Serial.printf("EOL\n");
		end_of_data = true;
		return true;
	}
	if (code == 2 && *len == 2) {
		// extended address, 20 bits
		if (!hex4(line + 9, &extaddr)) return false;
		if (!hex2(line + 11, &cksum)) return false;
		sum += (extaddr >> 8) + (extaddr & 255);
		if (((sum + cksum) & 255) != 0) return false;
		extended_addr = extaddr << 4;
		*len = 0;
		*addr = 0;
		//Serial.printf("ext addr = %08x\n", extended_addr);
		return true;
	}
	if (code == 4 && *len == 2) {
		// extended address, 20 bits
		if (!hex4(line + 9, &extaddr)) return false;
		if (!hex2(line + 11, &cksum)) return false;
		sum += (extaddr >> 8) + (extaddr & 255);
		if (((sum + cksum) & 255) != 0) return false;
		extended_addr = extaddr << 16;
		*len = 0;
		*addr = 0;
		//Serial.printf("ext addr = %08x\n", extended_addr);
		return true;
	}
	if (code == 0) {
		// normal data line
		if (*len > maxlen) return false;
		line += 9;
		for (uint32_t n=*len; n > 0; n--) {
			uint32_t byte;
			if (!hex2(line, &byte)) return false;
			line += 2;
			*data++ = byte;
			sum += byte;
		}
		if (!hex2(line, &cksum)) return false;
		if (((sum + cksum) & 255) != 0) return false;
		//Serial.printf("data at %04X, len=%d\n", *addr, *len);
		return true;
	}
	return false;
}

bool ihex_open(const char *filename)
{
	if (filename == NULL) return false;
	hexfile = SD.open(filename);
	if (!hexfile) return false;
	buflen = 0;
	extended_addr = 0;
	end_of_data = false;
	linebuf[0] = 0;
	return true;
}

bool ihex_read(uint32_t addr, uint8_t *data, uint32_t len, uint32_t *count)
{
	//Serial.printf("ihex_read:\n");
	if (end_of_data) return false;
	if (len == 0) return true;
	if (data == NULL) return false;
	memset(data, 0xFF, len);

	while (1) {
		if (linebuf[0]) {
			//Serial.printf("linebuf: %s\n", linebuf);
			uint32_t hexaddr, hexlen;
			uint8_t hexdata[64];
			if (!parse_hex(linebuf, &hexaddr, hexdata, sizeof(hexdata), &hexlen))
				return false;
			if (end_of_data) return true;
			if ((hexaddr < addr + len) && (hexaddr + hexlen > addr)) {
				// this line contains at least some data we need
				//Serial.printf("data: hexaddr=%04X, len=%d", hexaddr, hexlen);
				//Serial.printf(", for addr=%04X, len=%d\n", addr, len);
				uint32_t src_offset, dst_offset, copylen;
				if (hexaddr < addr) {
					src_offset = addr - hexaddr;
				} else {
					src_offset = 0;
				}
				dst_offset = hexaddr - addr + src_offset;
				copylen = hexlen - src_offset;
				if (hexaddr + hexlen > addr + len) {
					copylen -= (hexaddr + hexlen) - (addr + len);
				}
				//Serial.printf("src_offset=%d, dst_offset=%d, copylen=%d\n\n",
					//src_offset, dst_offset, copylen);
				memcpy(data + dst_offset, hexdata + src_offset, copylen);
				if (count) *count += copylen;
				if (hexaddr + hexlen >= addr + len) {
					// this line completed the requested region
					return true;
				}
			} else if (hexaddr + hexlen <= addr) {
				// this line is entirely before requested region
			} else {
				// this line is entirely after requested region
				return true;
			}
			linebuf[0] = 0;
		}
		if (!getline(hexfile, linebuf, sizeof(linebuf))) return false;
	}

	//while (getline(hexfile, linebuf, sizeof(linebuf))) {
		//parse_hex(linebuf, &hexaddr, hexdata, sizeof(hexdata), &hexlen);
	//}
	return false;
}

bool ihex_end(void)
{
	return end_of_data;
}

void ihex_close(void)
{
	hexfile.close();
}


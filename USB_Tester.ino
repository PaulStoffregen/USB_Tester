// AREF = 2.5V reference
// 24 = Fail LED
// 25 = Pass LED
// 2 = Power enable, high=on
// 3 = Overcurrent detect, low=overcurrent, open collector
// 8,9 = Amplifier gain
//   8=low  9=low  25X   200mA
//   8=low  9=high 50X   100mA
//   8=high 9=low  100X   50mA
//   8=high 9=high 200X   25mA
// A10 = Amplifier signal
// A11 = Amplifier reference

// build with Teensyduino 1.41-beta or later

#include <SD.h>
#include <USBHost_t36.h>
#include "bootloader.h"
#include "ihex.h"

USBHost usb;
USBHub hub1(usb);
TeensyBootloader bootloader(usb);
TeensyRawhid rawhid(usb);

#if F_CPU != 120000000
#error "Please compile with Tools > CPU set to 120 MHz"
#endif

const uint8_t unused_pins[] = {0, 1, 4, 5, 6, 7, 10, 11, 12, 13,
	14, 15, 16, 17, 18, 19, 20, /*21,*/ 22, 26, 27, 28, 29, 30, 31, 32,
	33, 34, 35, 36, 37, 38, 39};

uint8_t usbdata[1088];
uint8_t hexdata[1024];
uint8_t serdata[64];
uint8_t priorserdata[64];

void setup()
{
	pinMode(2, OUTPUT);
	pinMode(3, INPUT_PULLUP);
	pinMode(8, OUTPUT);
	pinMode(9, OUTPUT);
	pinMode(24, OUTPUT);
	pinMode(25, OUTPUT);
	digitalWrite(5, LOW);
	digitalWrite(24, LOW);
	digitalWrite(25, LOW);
	analogWrite(A21, 0);
	analogWrite(A22, 0);
	for (unsigned int i=0; i < sizeof(unused_pins); i++) {
		pinMode(unused_pins[i], OUTPUT);
		digitalWrite(unused_pins[i], LOW);
	}
	analogReadResolution(16);
	analogReadAveraging(32);
	analogReference(EXTERNAL);
	digitalWrite(8, LOW);
	digitalWrite(9, LOW);
	delay(100);
	bool r = SD.begin(BUILTIN_SDCARD);
	while (!Serial && millis() < 1500) ; // wait
	Serial.println("USB Tester, files present:");
	if (!r) sd_error();
	File rootdir = SD.open("/");
	if (!rootdir) sd_error();
	while (1) {
		File f = rootdir.openNextFile();
		if (!f) break;
		Serial.print("  ");
		Serial.println(f.name());
		f.close();
	}
	rootdir.close();
	digitalWrite(2, HIGH); // power on to USB
	usb.begin();
	delay(50);
}



float read_current(void)
{
	// returns current in mA
	// anything below 2mA should be considered zero

	float range = 200;
	int a10=0, a11=0;
	//int a7=0;
	for (int i=0; i<10; i++) {
		a10 += analogRead(A10);
		a11 += analogRead(A11);
		//a7 += analogRead(A7);
	}
	//Serial.printf("a10 = %d\n", a10 / 10);
	//Serial.printf("a11 = %d\n", a11 / 10);
	//Serial.printf("a7  = %d\n", a7 / 10);
	return (float)(a10 - a11) * range * (1.0 / 65536.0 / 10.0);
// A10 = Amplifier signal
// A11 = Amplifier reference
}


// wait for bootloader to appear (requires user to press button)
bool wait_for_bootloader()
{
	int undercurrent=0;

	while (undercurrent < 15) {
		if (bootloader) return true;
		if (read_current() < 0.3) {
			undercurrent++;
		} else {
			undercurrent = 0;
		}
	}
	return false;
}

bool identify_teensy_model(const char * &filename)
{
	Serial.println("Identify Teensy Model");
	elapsedMillis timeout = 0;
	while (timeout < 250) {
		uint8_t id = bootloader.id();
		switch (id) {
			case 0x00: break;
			case 0x1B: filename = "TEENSY20.HEX"; return true;
			case 0x1C: filename = "TEENSYPP.HEX"; return true;
			case 0x1D: filename = "TEENSY30.HEX"; return true;
			case 0x1E: filename = "TEENSY32.HEX"; return true; // T3.1
			case 0x1F: filename = "TEENSY35.HEX"; return true;
			case 0x20: filename = "TEENSYLC.HEX"; return true;
			case 0x21: filename = "TEENSY32.HEX"; return true;
			case 0x22: filename = "TEENSY36.HEX"; return true;
			default: return false; // unknown
		}
	}
	return false;
}

// program blink firmware
bool program_teensy(const char *filename)
{
	Serial.print("Program Teensy with ");
	if (!bootloader || bootloader.status() != 1) {
		Serial.println(" error, not running bootloader");
		return false;
	}
	Serial.println(filename);
	bool is_teensy20 = false;
	bool is_teensypp = false;
	uint32_t blocksize = 1024;
	uint32_t writesize;
	if (strcmp(filename, "TEENSY20.HEX") == 0) {
		is_teensy20 = true;
		blocksize = 128;
	}
	if (strcmp(filename, "TEENSYPP.HEX") == 0) {
		is_teensypp = true;
		blocksize = 256;
	}
	if (strcmp(filename, "TEENSYLC.HEX") == 0) {
		blocksize = 512;
	}

	if (!ihex_open(filename)) {
		Serial.println(" error opening file from SD card");
		return false;
	}
	memset(usbdata, 0, sizeof(usbdata));
	uint32_t address=0;
	// pre-read the first block
	bool ok = ihex_read(address, hexdata, blocksize);
	bool last = false;
	bool complete = false;
	bool first_write = true;
	while (ok && !complete) {
		//Serial.printf("hex address %x\n", address);
		//for (uint32_t i=0; i < 1024; i++) {
			//Serial.printf(" %02X", mydata[i+64]);
			//if ((i & 15) == 15) Serial.println();
		//}
		// send the data to USB
		if (is_teensy20) {
			usbdata[0] = address;
			usbdata[1] = address >> 8;
			memcpy(usbdata+2, hexdata, blocksize);
			writesize = blocksize + 2;
			bootloader.write(usbdata, writesize);
		} else if (is_teensypp) {
			usbdata[0] = address >> 8;
			usbdata[1] = address >> 16;
			memcpy(usbdata + 2, hexdata, blocksize);
			writesize = blocksize + 2;
			bootloader.write(usbdata, writesize);
		} else {
			usbdata[0] = address;
			usbdata[1] = address >> 8;
			usbdata[2] = address >> 16;
			memset(usbdata + 3, 0, 61);
			memcpy(usbdata + 64, hexdata, blocksize);
			writesize = blocksize + 64;
			bootloader.write(usbdata, writesize);
		}
		Serial.print(".");
		// read the next block from the file while USB sends
		if (last) {
			complete = true;
		} else {
			if (ihex_read(address + blocksize, hexdata, blocksize)) {
				if (ihex_end()) {
					//Serial.print(" end of file");
					last = true;
				}
			} else {
				Serial.print(" error reading file");
				ok = false;
			}
		}
		// wait for USB write
		elapsedMillis timeout = 0;
		uint32_t maxtime = 1000;
		if (first_write) {
			maxtime = 5000;
			first_write = false;
		}
		while (ok) {
			int status;
			do {
				status = bootloader.status();
				if (timeout > maxtime) {
					ok = false; // timeout
					break;
				}
			} while (status == 3);
			if (status == 1) {
				// successful write
				break;
			} else if (status == 2) { // teensy is busy
				if (timeout <= maxtime) {
					// keep retring every 10 ms
					delay(10);
					bootloader.write(usbdata, writesize);
				} else {
					// give up if too long
					ok = false;
				}
			} else {
				// USB disconnected, or other error
				ok = false;
			}
		}
		address += blocksize;
	}
	ihex_close();
	Serial.println();
	return ok;
}

// send reboot command, wait for bootloader disconnect
bool reboot_teensy(const char *filename)
{
	Serial.println("Reboot Teensy");
	if (!bootloader || bootloader.status() != 1) {
		Serial.println(" error, not running bootloader");
		return false;
	}
	uint32_t writesize;
	if (strcmp(filename, "TEENSY20.HEX") == 0) {
		writesize = 128 + 2;
	} else if (strcmp(filename, "TEENSYPP.HEX") == 0) {
		writesize = 256 + 2;
	} else if (strcmp(filename, "TEENSYLC.HEX") == 0) {
		writesize = 512 + 64;
	} else {
		writesize = 1024 + 64;
	}
	usbdata[0] = 0xFF;
	usbdata[1] = 0xFF;
	usbdata[2] = 0xFF;
	memset(usbdata + 3, 0, writesize - 3);

	bootloader.write(usbdata, writesize);
	elapsedMillis timeout = 0;
	while (1) {
		int status;
		do {
			status = bootloader.status();
			if (timeout > 500) return false;
		} while (status == 3);
		if (status == 1 || status == 0) {
			break;
		} else if (status == 2) { // teensy is busy
			if (timeout <= 500) {
				// keep retring every 10 ms
				delay(10);
				bootloader.write(usbdata, writesize);
			} else {
				// give up if too long
				return false;
			}
		} else {
			return false;
		}
	}
	while (timeout < 500) {
		if (!bootloader) return true;
	}
	Serial.println(" timeout waiting for bootloader offline");
	return false;
}

// wait for rawhid to appear
bool wait_for_rawhid()
{
	Serial.println("Wait for RawHID Device");
	elapsedMillis timeout = 0;
	while (timeout < 700) {
		if (rawhid) return true;
	}
	Serial.println(" timeout waiting for rawhid");
	return false;
}

// use rawhid to test LED, turn on/off and measure current
bool test_led()
{
	Serial.println("Test LED");
	if (!rawhid) return false;

	usbdata[0] = 0;
	memset(usbdata + 1, 0, 63);
	rawhid.write(usbdata);
	for (int i=0; i < 50; i++) { read_current(); delay(1); }
	float led_off_mA = read_current();
	Serial.print("  off mA: ");
	Serial.println(led_off_mA);
	usbdata[0] = 1;
	memset(usbdata + 1, 0, 63);
	rawhid.write(usbdata);
	for (int i=0; i < 50; i++) { read_current(); delay(1); }
	float led_on_mA = read_current();
	Serial.print("   on mA: ");
	Serial.println(led_on_mA);
	float diff = led_on_mA - led_off_mA;
	if (diff < 2.0 || diff > 4.0) return false;
	return true;
}

// read serial number and Freescale ID number
bool read_id_bytes()
{
	Serial.println("Read ID Bytes");
	if (!rawhid) return false;
	usbdata[0] = 1;
	usbdata[1] = 0x5A;
	memset(usbdata + 2, 0, 62);
	rawhid.write(usbdata);
	elapsedMillis timeout = 0;
	while (1) {
		if (rawhid.read(serdata)) break;
		if (timeout > 400) return false;
	}
	uint32_t len = serdata[0];
	if (len < 12 || len > 48) return false;
	for (unsigned int i=0; i < len; i++) {
		Serial.print(serdata[i+1], HEX);
		if (i < len-1) Serial.print(",");
	}
	Serial.println();
	return true;
}

char hex(uint32_t n) {
	n &= 15;
	if (n < 10) return '0' + n;
	return 'A' + n - 10;
}

// copy the filename and id bytes to "hexdata" in ascii format
void format_id_bytes(const char *filename)
{
	if (memcmp(serdata, priorserdata, 64) != 0) {
		memcpy(priorserdata, serdata, 64);
		strcpy((char *)hexdata, filename);
		int len = strlen(filename);
		int datalen = serdata[0];
		for (int i=0; i < datalen; i++) {
			hexdata[len++] = ',';
			hexdata[len++] = hex(serdata[i+1] >> 4);
			hexdata[len++] = hex(serdata[i+1]);
		}
		hexdata[len++] = 0;
		Serial.print("log: ");
		Serial.println((char *)hexdata);
	} else {
		hexdata[0] = 0;
		Serial.println("previously tested, no need to log");
	}
}

void store_id_bytes(const char *filename)
{
	if (!hexdata[0]) return;
	File logfile = SD.open("log.txt", FILE_WRITE);
	if (!logfile) sd_error();
	logfile.write((char *)hexdata);
	logfile.close();
}

bool runtest()
{
	const char *filename;

	if (!wait_for_bootloader()) return false;
	if (!identify_teensy_model(filename)) return false;
	if (!program_teensy(filename)) return false;
	if (!reboot_teensy(filename)) return false;
	if (!wait_for_rawhid()) return false;
	if (!test_led()) return false;
	if (!read_id_bytes()) return false;
	format_id_bytes(filename);
	return true;
}

void loop()
{
	// wait for a device to appear
	float mA = read_current();
	static unsigned int testcount=0;

	if (mA > 0.5 || bootloader || rawhid) {
		Serial.println();
		//uint32_t d, p, t, s;
		//usb.countFree(d, p, t, s);
		//Serial.printf("memory = %d,%d,%d,%d\n", d, p, t, s);
		Serial.print("USB Device Detected, test #");
		Serial.println(++testcount);
		// turn off the LEDs
		digitalWrite(24, LOW);
		digitalWrite(25, LOW);
		// run the tests
		bool r = runtest();
		if (r) {
			digitalWrite(25, HIGH); // Green LED
			Serial.println("Pass");
		} else {
			digitalWrite(24, HIGH); // Red LED
			Serial.println("Fail");
		}
#if 1
		// write serial number to log file
		store_id_bytes("log.txt");
		// wait for no current (cable unplug)
		int undercurrent=0;
		while (1) {
			mA = read_current();
			//Serial.println(mA);
			if (mA < 0.3) {
				if (++undercurrent > 15) break;
			} else {
				undercurrent = 0;
			}
		}
		Serial.println("USB unplugged");
#endif
	}
	//Serial.printf("current = %.2f\n", read_current());
	//delay(1000);
}

void sd_error(void)
{
	digitalWrite(25, LOW);
	while (1) {
		Serial.println("can't access SD card");
		digitalWrite(24, HIGH);
		delay(100);
		digitalWrite(24, LOW);
		delay(150);
	}
}



// prior hardware
// 23 = current measurement, 100 mA reads as 1.0V
// PTE6 = USB power, high = enable
// A21 = overcurrent threshold (DAC voltage)
// 4 = overcurrent alert, open collector, low=alert
// 5 = overcurrent reset, low=direct, high=latch any alert

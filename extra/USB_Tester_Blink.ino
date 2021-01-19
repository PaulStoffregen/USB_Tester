#if defined(__AVR__)
#include <avr/boot.h>
#if F_CPU != 8000000
#error "Please set CPU Speed to 8 MHz"
#endif
#elif defined(__arm__) && (defined(KINETISK) || defined(KINETISL))
#if F_CPU != 24000000
#error "Please set CPU Speed to 24 MHz"
#endif
#endif

#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MKL26Z64__)
  #define NUM_SOLDERED_DIGITAL_PINS 24
  #define NUM_SOLDERED_TOTAL_PINS   24
#elif defined(__MK64FX512__) || defined(__MK66FX1M0__)
  #define NUM_SOLDERED_DIGITAL_PINS 40
  #define NUM_SOLDERED_TOTAL_PINS   42
#elif defined(__IMXRT1062__) && defined(ARDUINO_TEENSY40)
  #define NUM_SOLDERED_DIGITAL_PINS 24
  #define NUM_SOLDERED_TOTAL_PINS   24
#elif defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
  #define NUM_SOLDERED_DIGITAL_PINS 42
  #define NUM_SOLDERED_TOTAL_PINS   42
#else
  #error "Need to define NUM_SOLDERED_DIGITAL_PINS for this board"
#endif

#include <util/crc16.h>

bool blinking = true;
elapsedMillis inactive;

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	#if defined(__arm__)
	Serial1.begin(750000);
	#endif
	//Serial2.begin(115200);
}

void mydelay(unsigned int msec) {
	uint8_t buf[64];
	elapsedMillis ms = 0;
	while (ms < msec) {
		// For USB port testing...
		if (RawHID.recv(buf, 0) > 0) {
			pinMode(LED_BUILTIN, OUTPUT);
			digitalWrite(LED_BUILTIN, ((buf[0] & 1) ? HIGH : LOW));
			//Serial2.printf("revc %d\r\n", buf[0]);
			ms = 0;
			if (buf[1] == 0x5A) {
				memset(buf, 0, 64);
				#if (defined(__IMXRT1062__))
				buf[0] = 24;
				memword(buf+1, HW_OCOTP_MAC0);
				memword(buf+5, HW_OCOTP_MAC1);
				memword(buf+9, HW_OCOTP_CFG0);
				memword(buf+13, HW_OCOTP_CFG1);
				memword(buf+17, HW_OCOTP_CFG2);
				memword(buf+21, HW_OCOTP_CFG3);
				#elif defined(KINETISK)
				buf[0] = 20;
				memword(buf+1, read_serial_number());
				memword(buf+5, SIM_UIDL);
				memword(buf+9, SIM_UIDML);
				memword(buf+13, SIM_UIDMH);
				memword(buf+17, SIM_UIDH);
				#elif defined(KINETISL)
				buf[0] = 16;
				memword(buf+1, read_serial_number());
				memword(buf+5, SIM_UIDL);
				memword(buf+9, SIM_UIDML);
				memword(buf+13, SIM_UIDMH);
				#elif defined(__AVR__)
				// 27.7.10 Reading the Signature Row from Software
				buf[0] = 36;
				buf[1] = 0x39;
				buf[2] = 0x30;
				buf[3] = 0x00;
				buf[4] = 0x00;
				noInterrupts();
				for (int i=0; i < 32; i++) {
					buf[i + 5] = boot_signature_byte_get(i);
				}
				interrupts();
				#endif
				RawHID.send(buf, 250);
			}
			blinking = false;
			inactive = 0;
		}
		#if defined(__arm__)
		// For testing soldered pins...
		if (Serial1.read() == 0xC9) {
			for (int pin=2; pin < NUM_SOLDERED_DIGITAL_PINS; pin++) {
				pinMode(pin, INPUT);
			}
			buf[0] = 0x36;
			memword(buf+1, read_serial_number());
			buf[5] = NUM_SOLDERED_TOTAL_PINS;
			buf[6] = read_pins(2);
			buf[7] = read_pins(10);
			buf[8] = read_pins(18);
			buf[9] = read_pins(26);
			buf[10] = read_pins(34);
			buf[11] = 0;
			buf[12] = 0;
			buf[13] = 0;
			uint16_t crc = 0x5A96;
			for (int i=0; i < 14; i++) {
				crc = _crc16_update(crc, buf[i]);
			}
			buf[14] = crc & 255;
			buf[15] = (crc >> 8) & 255;
			Serial1.write(buf, 16);
			blinking = false;
			inactive = 0;
		}
		#endif
		if (!blinking && inactive > 1000) {
			pinMode(LED_BUILTIN, OUTPUT);
			blinking = true;
		}
	}
}

void loop() {
	if (blinking) digitalWrite(LED_BUILTIN, HIGH);
	mydelay(1000);
	if (blinking) digitalWrite(LED_BUILTIN, LOW);
	mydelay(1000);
}

#if defined(__arm__)
uint32_t read_serial_number() {
	uint32_t num;
	__disable_irq();
#if defined(HAS_KINETIS_FLASH_FTFA) || defined(HAS_KINETIS_FLASH_FTFL)
	FTFL_FSTAT = FTFL_FSTAT_RDCOLERR | FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL;
	FTFL_FCCOB0 = 0x41;
	FTFL_FCCOB1 = 15;
	FTFL_FSTAT = FTFL_FSTAT_CCIF;
	while (!(FTFL_FSTAT & FTFL_FSTAT_CCIF)) ; // wait
	num = *(uint32_t *)&FTFL_FCCOB7;
#elif defined(HAS_KINETIS_FLASH_FTFE)
	kinetis_hsrun_disable();
	FTFL_FSTAT = FTFL_FSTAT_RDCOLERR | FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL;
	*(uint32_t *)&FTFL_FCCOB3 = 0x41070000;
	FTFL_FSTAT = FTFL_FSTAT_CCIF;
	while (!(FTFL_FSTAT & FTFL_FSTAT_CCIF)) ; // wait
	num = *(uint32_t *)&FTFL_FCCOBB;
	kinetis_hsrun_enable();
#else
	return HW_OCOTP_MAC0;
#endif
	__enable_irq();
	return num;
}

uint8_t read_pins(int first_pin)
{
	int b = 0;
	for (int i=0; i < 8; i++) {
		int pin = first_pin + i;
		if (pin < NUM_SOLDERED_DIGITAL_PINS) {
			if (digitalRead(pin)) b |= (1 << i);
		}
		#if defined(__MK64FX512__) || defined(__MK66FX1M0__)
		else if (pin == 40 && analogRead(A21) >= 512) b |= (1 << i);
		else if (pin == 41 && analogRead(A22) >= 512) b |= (1 << i);
		#endif
	}
	return b;
}
#endif

void memword(uint8_t *dest, uint32_t n)
{
	*dest++ = n & 255;
	*dest++ = (n >> 8) & 255;
	*dest++ = (n >> 16) & 255;
	*dest++ = (n >> 24) & 255;
}

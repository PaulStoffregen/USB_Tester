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
		if (RawHID.recv(buf, 0) > 0) {
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
		}
		#if defined(__arm__)
		// For testing soldered pins...
		if (Serial1.read() == 0xC9) {
			buf[0] = 0x36;
			memword(buf+1, read_serial_number());
			buf[5] = read_pins(2);
			buf[6] = read_pins(10);
			buf[7] = read_pins(18);
			buf[8] = read_pins(26);
			buf[9] = read_pins(34);
			buf[10] = 0;
			buf[11] = 0;
			buf[12] = 0;
			buf[13] = 0;
			buf[14] = 0;
			buf[15] = 0;
			Serial1.write(buf, 16);
		}
		#endif
	}
}

void loop() {
	digitalWrite(LED_BUILTIN, HIGH);
	mydelay(1000);
	digitalWrite(LED_BUILTIN, LOW);
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
		if (pin < NUM_DIGITAL_PINS) {
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

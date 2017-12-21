#if defined(__AVR__)
#include <avr/boot.h>
#if F_CPU != 8000000
#error "Please set CPU Speed to 8 MHz"
#endif
#elif defined(__arm__)
#if F_CPU != 24000000
#error "Please set CPU Speed to 24 MHz"
#endif
#endif

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	//Serial1.begin(115200);
}

void mydelay(unsigned int msec) {
	uint8_t buf[64];
	elapsedMillis ms = 0;
	while (ms < msec) {
		if (RawHID.recv(buf, 0) > 0) {
			digitalWrite(LED_BUILTIN, ((buf[0] & 1) ? HIGH : LOW));
			//Serial1.printf("revc %d\r\n", buf[0]);
			ms = 0;
			if (buf[1] == 0x5A) {
				memset(buf, 0, 64);
				#if defined(KINETISK)
				buf[0] = 20;
				uint32_t serailnum = read_serial_number();
				memcpy(buf+1, &serailnum, 4);
				memcpy(buf+5, &SIM_UIDL, 4);
				memcpy(buf+9, &SIM_UIDML, 4);
				memcpy(buf+13, &SIM_UIDMH, 4);
				memcpy(buf+17, &SIM_UIDH, 4);
				#elif defined(KINETISL)
				buf[0] = 16;
				uint32_t serailnum = read_serial_number();
				memcpy(buf+1, &serailnum, 4);
				memcpy(buf+5, &SIM_UIDL, 4);
				memcpy(buf+9, &SIM_UIDML, 4);
				memcpy(buf+13, &SIM_UIDMH, 4);
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
#endif
	__enable_irq();
	return num;
}
#endif

#include "bootloader.h"

void TeensyBootloader::init()
{
	state = 0;
	memset(hiddesc, 0, sizeof(hiddesc));
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	driver_ready_for_device(this);
}

bool TeensyBootloader::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	//Serial.printf("TeensyBootloader claim this=%08X\n", (uint32_t)this);
	if (type != 0) return false; // only claim at device level
	if (dev->idVendor != 0x16C0) return false;
	if (dev->idProduct != 0x0478) return false;
	//Serial.printf("TeensyBootloader, ID=16C0/0478, millis=%u\n", millis());

	if (len < 25) return false;
	if (descriptors[0] != 9) return false; // len=9
	if (descriptors[1] != 4) return false; // interface
	if (descriptors[5] != 3) return false; // hid
	if (descriptors[9] != 9) return false; // len=9
	if (descriptors[10] != 0x21) return false; // HID
	if (descriptors[15] != 0x22) return false; // HID descriptor
	uint32_t size = descriptors[16] | (descriptors[17] << 8);
	if (size < 12 || size > sizeof(hiddesc)) return false;
	//for (unsigned int i=0; i<len; i++) Serial.printf("d[%d] = %02X\n", i, descriptors[i]);

	memset(hiddesc, 0, sizeof(hiddesc));
	mk_setup(setup, 0x81, 6, 0x2200, 0, size); // 6=GET_DESCRIPTOR
	queue_Control_Transfer(dev, &setup, hiddesc, this);
	state = 0;
	return true;
}

void TeensyBootloader::control(const Transfer_t *transfer)
{
	//Serial.printf("TeensyBootloader control\n");
	if (transfer->setup.bRequest == 6 && transfer->setup.wValue == 0x2200) {
		//Serial.printf("TeensyBootloader HID Descriptor\n");
		for (unsigned int i=0; i < transfer->setup.wLength; i++) {
			//Serial.printf("hid[%d] = %02X\n", i, hiddesc[i]);
		}
		state = 1;
		return;
	}
	if (transfer->setup.bRequest == 9 && transfer->setup.wValue == 0x0200) {
		uint32_t token = transfer->qtd.token;
		//Serial.printf("TeensyBootloader write callback, token=%08X\n", token);
		if ((token & 0x40) == 0) {
			// normal, not active, not halted
			state = 1;
		} else {
			// halted, stall?
			//Serial.printf(" HALT BIT\n");
			state = 2;
		}
	}
}

uint8_t TeensyBootloader::id()
{
	if (!device) return 0;
	if (state == 0) return 0;
	volatile uint8_t *idbyte = hiddesc + 4;
	//return hiddesc[4];
	return *idbyte;
}

void TeensyBootloader::write(uint8_t *data, uint32_t len)
{
	mk_setup(setup, 0x21, 9, 0x0200, 0, len);
	queue_Control_Transfer(device, &setup, data, this);
	state = 3;
}

void TeensyBootloader::disconnect()
{
	memset(hiddesc, 0, sizeof(hiddesc));
	//Serial.println("TeensyBootloader disconnect");
	state = 0;
}




void TeensyRawhid::init()
{
	state = 0;
	driver_ready_for_device(this);
}

bool TeensyRawhid::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
        //Serial.printf("TeensyRawhid claim this=%08X\n", (uint32_t)this);
	if (type != 0) return false; // only claim at device level
	if (dev->idVendor != 0x16C0) return false;
	if (dev->idProduct != 0x0486) return false;
	//Serial.printf("TeensyRawhid, ID=16C0/0486, millis=%u\n", millis());
	//for (unsigned int i=0; i<len; i++) Serial.printf("d[%d] = %02X\n", i, descriptors[i]);
	uint8_t rawhid_rx_ep = 3; // 64 bytes
	uint8_t rawhid_tx_ep = 4; // 64 bytes
	//uint8_t seremu_rx_ep = 1; // 64 bytes
	//uint8_t seremu_tx_ep = 2; // 32 bytes

	// we already know these descriptors, but reading them allows the
	// USB protocol analyzer to know the HID report and show non-error status
	uint32_t size = descriptors[16] | (descriptors[17] << 8);
	uint32_t size2 = descriptors[48] | (descriptors[49] << 8);

	if (size > sizeof(desc) || size2 > sizeof(desc2)) return false;
	mk_setup(setup, 0x81, 6, 0x2200, 0, size); // 6=GET_DESCRIPTOR
	queue_Control_Transfer(dev, &setup, desc, this);
	mk_setup(setup2, 0x81, 6, 0x2200, 1, size2); // 6=GET_DESCRIPTOR
	queue_Control_Transfer(dev, &setup2, desc2, this);
	//Serial.println("TeensyRawhid claim middle");

	rawhid_tx_pipe = new_Pipe(dev, 3, rawhid_tx_ep, 0, 64, 1);
	//Serial.printf("  rawhid_tx_pipe = %08X\n", (uint32_t)rawhid_tx_pipe); // NULL on 43rd run
	rawhid_tx_pipe->callback_function = tx_callback; // crashes HERE!!!!!!!!!!!!!!!!

	//Serial.println("TeensyRawhid claim middle2");

	rxbuffull = false;
	rawhid_rx_pipe = new_Pipe(dev, 3, rawhid_rx_ep, 1, 64, 1);
	rawhid_rx_pipe->callback_function = rx_callback;

	//Serial.println("TeensyRawhid claim middle3");

	queue_Data_Transfer(rawhid_rx_pipe, rxbuf, 64, this);

	state = 1;
	//Serial.println("TeensyRawhid claim ok");
	return true;
}

void TeensyRawhid::write(uint8_t *buffer)
{
	queue_Data_Transfer(rawhid_tx_pipe, buffer, 64, this);
}

void TeensyRawhid::tx_callback(const Transfer_t *transfer)
{
	//Serial.printf("TeensyRawhid data callback, ms=%u\n", millis());
}

void TeensyRawhid::rx_callback(const Transfer_t *transfer)
{
	//Serial.printf("TeensyRawhid receive data callback\n");
	//rxbuffull = true;
	((TeensyRawhid *)(transfer->driver))->rxbuffull = true;
}

bool TeensyRawhid::read(uint8_t *buffer)
{
	if (!rxbuffull) return false;
	memcpy(buffer, rxbuf, 64);
	rxbuffull = false;
	queue_Data_Transfer(rawhid_rx_pipe, rxbuf, 64, this);
}


void TeensyRawhid::disconnect()
{
	state = 0;
}

bool TeensyRawhid::reboot_command()
{
	if (!device) return false;
	rebootcmdbuf[0] = 0xA9;
	rebootcmdbuf[1] = 0x45;
	rebootcmdbuf[2] = 0xC2;
	rebootcmdbuf[3] = 0x6B;
	mk_setup(setup, 0x21, 9, 0x0300, 1, 4);
	queue_Control_Transfer(device, &setup, rebootcmdbuf, this);
	return true;
}




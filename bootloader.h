#include <Arduino.h>
#include "USBHost_t36.h"

class TeensyBootloader : public USBDriver {
public:
	TeensyBootloader(USBHost &host) { init(); }
	uint8_t id();
	void write(uint8_t *data, uint32_t len);
	uint8_t status() { return state; } // 0=no device, 1=idle, 2=busy, 3=writing
protected:
	virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	virtual void control(const Transfer_t *transfer);
	virtual void disconnect();
	static void callback(const Transfer_t *transfer);
	void init();
private:
	Pipe_t mypipes[6] __attribute__ ((aligned(32)));
	Transfer_t mytransfers[40] __attribute__ ((aligned(32)));
	setup_t setup;
	uint8_t hiddesc[32];
	volatile uint8_t state;
};


class TeensyRawhid: public USBDriver {
public:
        TeensyRawhid(USBHost &host) { init(); }
	void write(uint8_t *buffer);
	bool read(uint8_t *buffer);
	bool reboot_command();
	uint8_t status() { return state; }
protected:
        virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
	void init();
	static void tx_callback(const Transfer_t *transfer);
	static void rx_callback(const Transfer_t *transfer);
	virtual void disconnect();
private:
	Pipe_t *rawhid_rx_pipe;
	Pipe_t *rawhid_tx_pipe;
	Pipe_t *seremu_rx_pipe;
	Pipe_t *seremu_tx_pipe;
	uint8_t rebootcmdbuf[4];
	uint8_t desc[40];
	setup_t setup;
	uint8_t desc2[40];
	setup_t setup2;
	volatile uint8_t state;
	uint8_t rxbuf[64];
	volatile bool rxbuffull;
};



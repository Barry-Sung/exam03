#include "mbed.h"

#include "math.h"

#include "mbed_rpc.h"

#include "fsl_port.h"

#include "fsl_gpio.h"

#include "MQTTNetwork.h"

#include "MQTTmbed.h"

#include "MQTTClient.h"

#define UINT14_MAX        16383

// FXOS8700CQ I2C address

#define FXOS8700CQ_SLAVE_ADDR0 (0x1E<<1) // with pins SA0=0, SA1=0

#define FXOS8700CQ_SLAVE_ADDR1 (0x1D<<1) // with pins SA0=1, SA1=0

#define FXOS8700CQ_SLAVE_ADDR2 (0x1C<<1) // with pins SA0=0, SA1=1

#define FXOS8700CQ_SLAVE_ADDR3 (0x1F<<1) // with pins SA0=1, SA1=1

// FXOS8700CQ internal register addresses

#define FXOS8700Q_STATUS 0x00

#define FXOS8700Q_OUT_X_MSB 0x01

#define FXOS8700Q_OUT_Y_MSB 0x03

#define FXOS8700Q_OUT_Z_MSB 0x05

#define FXOS8700Q_M_OUT_X_MSB 0x33

#define FXOS8700Q_M_OUT_Y_MSB 0x35

#define FXOS8700Q_M_OUT_Z_MSB 0x37

#define FXOS8700Q_WHOAMI 0x0D

#define FXOS8700Q_XYZ_DATA_CFG 0x0E

#define FXOS8700Q_CTRL_REG1 0x2A

#define FXOS8700Q_M_CTRL_REG1 0x5B

#define FXOS8700Q_M_CTRL_REG2 0x5C

#define FXOS8700Q_WHOAMI_VAL 0xC7


I2C i2c( PTD9,PTD8);

RawSerial pc(USBTX, USBRX);

RawSerial xbee(D12, D11);

int m_addr = FXOS8700CQ_SLAVE_ADDR1;


void FXOS8700CQ_readRegs(int addr, uint8_t * data, int len);

void FXOS8700CQ_writeRegs(uint8_t * data, int len);

void getAcc();

// rpc functions & variables
void getData(Arguments *in, Reply *out);
RPCFunction rpcAcc(&getData, "getData");
char buf[256], outbuf[256];
int query_count=0;

//xbee functions & variables
void xbee_setup(void);
void xbee_rx_interrupt(void);
void xbee_rx(void);
void reply_messange(char *xbee_reply, char *messange);
void check_addr(char *xbee_reply, char *messenger);
Thread t;
EventQueue queue;

//accel variables
uint8_t data[2] ;
int num_data=0;                       
int tilt[100]={0};                      
int i=0;                                  
Thread t_velocity;
EventQueue queue_velocity;
void getVelo(void);
float X,Y,Z;
float velo=0;


int main() {

  pc.baud(9600);

  xbee_setup();

  t_velocity.start(callback(&queue_velocity, &EventQueue::dispatch_forever));
  // Get Velocity every 0.1 s
  queue_velocity.call_every(100, &getVelo);

  t.start(callback(&queue, &EventQueue::dispatch_forever));
  // Setup a serial interrupt function of receiving data from xbee
  xbee.attach(xbee_rx_interrupt, Serial::RxIrq);


   // Enable the FXOS8700Q

  FXOS8700CQ_readRegs( FXOS8700Q_CTRL_REG1, &data[1], 1);

  data[1] |= 0x01;

  data[0] = FXOS8700Q_CTRL_REG1;

  FXOS8700CQ_writeRegs(data, 2);

}
void getVelo(){
  
  getAcc();
  velo = sqrt(X*X+Y*Y)*0.1;
  pc.printf("velocity=%f\r\n",velo);
  // for(int i=0; i<100; i++){
  //   xbee.printf("%1.2f\r\n %1.2f\r\n %1.2f\r\n %d\r\n", X[i], Y[i], Z[i], tilt[i]);
  //   pc.printf("%1.2f %1.2f %1.2f %d\r\n", X[i], Y[i], Z[i], tilt[i]);
  // }
}

void xbee_setup(){

  char xbee_reply[4];

  xbee.baud(9600);

  xbee.printf("+++");

  xbee_reply[0] = xbee.getc();

  xbee_reply[1] = xbee.getc();

  if(xbee_reply[0] == 'O' && xbee_reply[1] == 'K'){

    pc.printf("enter AT mode.\r\n");

    xbee_reply[0] = '\0';

    xbee_reply[1] = '\0';

  }

  xbee.printf("ATMY 0x240\r\n");

  reply_messange(xbee_reply, "setting MY : 0x240");


  xbee.printf("ATDL 0x140\r\n");

  reply_messange(xbee_reply, "setting DL : 0x140");


  xbee.printf("ATID 0x1\r\n");

  reply_messange(xbee_reply, "setting PAN ID : 0x1");


  xbee.printf("ATWR\r\n");

  reply_messange(xbee_reply, "write config");


  xbee.printf("ATMY\r\n");

  check_addr(xbee_reply, "MY");


  xbee.printf("ATDL\r\n");

  check_addr(xbee_reply, "DL");


  xbee.printf("ATCN\r\n");

  reply_messange(xbee_reply, "exit AT mode");
}

void xbee_rx_interrupt(void)
{

  xbee.attach(NULL, Serial::RxIrq); // detach interrupt

  queue.call(&xbee_rx);

}

void xbee_rx(){

  while(xbee.readable()){

    memset(buf, 0, 256);      // clear buffer

    for(int i=0; i<255; i++) {

        char recv = xbee.getc();

        if ( recv == '\r' || recv == '\n' ) {

          xbee.printf("\r\n");

          break;

        }

        buf[i] = xbee.putc(recv);
              
    }

    RPC::call(buf, outbuf);
    
    num_data = 0;

    pc.printf("outbuf = %s\r\n", outbuf);

  }
  xbee.attach(xbee_rx_interrupt, Serial::RxIrq);

}
void getData(Arguments *in, Reply *out){
  
  xbee.printf("%f\r\n",velo);
}

void getAcc() {

   int16_t acc16;

   uint8_t res[6];

   FXOS8700CQ_readRegs(FXOS8700Q_OUT_X_MSB, res, 6);

   acc16 = (res[0] << 6) | (res[1] >> 2);

   if (acc16 > UINT14_MAX/2)

      acc16 -= UINT14_MAX;

   X = ((float)acc16) / 4096.0f;


   acc16 = (res[2] << 6) | (res[3] >> 2);

   if (acc16 > UINT14_MAX/2)

      acc16 -= UINT14_MAX;

   Y = ((float)acc16) / 4096.0f;


   acc16 = (res[4] << 6) | (res[5] >> 2);

   if (acc16 > UINT14_MAX/2)

      acc16 -= UINT14_MAX;

   Z= ((float)acc16) / 4096.0f;

 
}


// void getAddr(Arguments *in, Reply *out) {

//    uint8_t who_am_i, data[2];

//    FXOS8700CQ_readRegs(FXOS8700Q_WHOAMI, &who_am_i, 1);

//    pc.printf("Here is %x", who_am_i);


// }


void FXOS8700CQ_readRegs(int addr, uint8_t * data, int len) {

   char t = addr;

   i2c.write(m_addr, &t, 1, true);

   i2c.read(m_addr, (char *)data, len);

}


void FXOS8700CQ_writeRegs(uint8_t * data, int len) {

   i2c.write(m_addr, (char *)data, len);

}

void reply_messange(char *xbee_reply, char *messange){

  xbee_reply[0] = xbee.getc();

  xbee_reply[1] = xbee.getc();

  xbee_reply[2] = xbee.getc();

  if(xbee_reply[1] == 'O' && xbee_reply[2] == 'K'){

    pc.printf("%s\r\n", messange);

    xbee_reply[0] = '\0';

    xbee_reply[1] = '\0';

    xbee_reply[2] = '\0';

  }

}


void check_addr(char *xbee_reply, char *messenger){

  xbee_reply[0] = xbee.getc();

  xbee_reply[1] = xbee.getc();

  xbee_reply[2] = xbee.getc();

  xbee_reply[3] = xbee.getc();

  pc.printf("%s = %c%c%c\r\n", messenger, xbee_reply[1], xbee_reply[2], xbee_reply[3]);

  xbee_reply[0] = '\0';

  xbee_reply[1] = '\0';

  xbee_reply[2] = '\0';

  xbee_reply[3] = '\0';

}


/*
 *  Copyright (C) 2002-2006  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include "config.h"
#include "setup.h"

#if C_DIRECTLPT

/* Linux version */
#if defined (LINUX)

#include "parport.h"
#include "directlpt_linux.h"
#include "callback.h"
#include <linux/ppdev.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <SDL.h>

CDirectLPT::CDirectLPT (Bitu nr, Bit8u initIrq, CommandLine* cmd)
                              :CParallel (cmd, nr, initIrq) {
	InstallationSuccessful = false;
	interruptflag=true; // interrupt disabled

	std::string str;
/*
if(!cmd->FindStringBegin("realport:",str,false)) {
		LOG_MSG("parallel%d: realport parameter missing.",nr+1);
	----77	return;
	}

	porthandle = open(str.c_str(), O_RDWR );
	if(porthandle < 0) {
		LOG_MSG("parallel%d: Could not open port %s.",nr+1,str.c_str());
		if (errno == 2) LOG_MSG ("The specified port does not exist.");
		else if(errno==EBUSY) LOG_MSG("The specified port is already in use.");
		else if(errno==EACCES) LOG_MSG("You are not allowed to access this port.");
		else LOG_MSG("Errno %d occurred.",errno);
		return;
	}

	if(ioctl( porthandle, PPCLAIM, NULL ) < 0) {
		LOG_MSG("parallel%d: failed to claim port.",nr+1);
		return;
	} */
	// TODO check return value
	
	// go for it
	porthandle= wiringPiI2CSetup (0x20) ;
	wiringPiI2CWriteReg8 (porthandle, 0x00, 0x00 ); //Set GPIOA ALL AS OUTPUT
	wiringPiI2CWriteReg8 (porthandle, 0x01,  0b01011110 ); //Set GPIOB 1=Read,0=OUT
	//IPOL GPIOB??
	LOG_MSG("putchar: no printer");
	ack_polarity=false;
	initialize();

	InstallationSuccessful = true;
}

CDirectLPT::~CDirectLPT () {
	//if(porthandle > 0) close(porthandle);
} 

bool CDirectLPT::Putchar(Bit8u val)
{	
	//LOG_MSG("putchar: %x",val);
	Write_CON(0xD4);

	// check if printer online and not busy
	// PE and Selected: no printer attached
	Bit8u sr=Read_SR();
	//LOG_MSG("SR: %x",sr);
	if((sr&0x30)==0x30)
	{
		LOG_MSG("putchar: no printer");
		//return false;
	}
	// error
	if(sr&0x20)
	{
		LOG_MSG("putchar: paper out");
		//return false;
	}
	if((sr&0x08)==0)
	{
		LOG_MSG("putchar: printer error");
		//return false;
	}
	// busy
	Bitu timeout = 10000;
	Bitu time = timeout+SDL_GetTicks();
	
	while(((sr&0x80)==0)&&(SDL_GetTicks()<time))
	{// wait for the printer to get ready
		CALLBACK_Idle();
		sr=Read_SR();
	}
	if(SDL_GetTicks()>=time)
	{
		LOG_MSG("putchar: busy timeout");
		return false;
	}
	// strobe data out
	Write_PR(val);
	//bool wasbusy;
	
	// I hope this creates a sufficient long pulse...
	// (I/O-Bus at 7.15 MHz will give some delay)
	for(int i = 0; i < 10; i++)
		Write_CON(0xD5); // strobe pulse

	Write_CON(0xD4); // strobe off

	// wait some time for ack
	/*
	Bitu z = 0;
	bool prevAck=false;
	while(z<1000)
	{
		if((READ_SR()&0x40)==0) prevAck=true;
		else 
	}
	*/
//int wiringPiI2CReadReg8 (int fd, int reg) ;
//int wiringPiI2CWriteReg8 (int fd, int reg, int data) ;
//#if PARALLEL_DEBUG
	log_par(dbg_putchar,"putchar  0x%2x",val);
	if(dbg_plainputchar) fprintf(debugfp,"%c",val);
//#endif
	return true;
}
Bitu CDirectLPT::Read_PR() {
	Bitu retval;
	//ioctl(porthandle, PPRDATA, &retval);
	return retval;
}
Bitu CDirectLPT::Read_COM() {
	Bitu retval;
	//ioctl(porthandle, PPRCONTROL, &retval);
	return retval;
}
Bitu CDirectLPT::Read_SR() {
	//Bitu retval;
	//ioctl(porthandle, PPRSTATUS, &retval);
	char b = wiringPiI2CReadReg8 (porthandle, 0x13);
	char retval=(b&2)<<5|(b&4)<<5|(b&8)<<2 |(b&16)|(b&64)>>3;
	LOG_MSG("printer raw status: %x",retval);
	LOG_MSG("printer oldraw statustesrt: %x",b);
	
	return retval;
}

void CDirectLPT::Write_PR(Bitu val){
	//ioctl(porthandle, PPWDATA, &val); 
	 wiringPiI2CWriteReg8 (porthandle, 0x12, val);
	 LOG_MSG("parallel%d: ",val); //Set all Datapins GPIOA
	}
void CDirectLPT::Write_CON(Bitu val) {
	//ioctl(porthandle, PPWCONTROL, &val); 
	 Bitu controlout = (val&1)|(val&2)<<4|(val&4)<<5; //sort Controlbyte to GPIOB Outputs, Automatic Write only outputs eg Register OLATB
     wiringPiI2CWriteReg8 (porthandle, 0x13,controlout);
	 }
void CDirectLPT::Write_IOSEL(Bitu val) {
	// switches direction old-style TODO
	if((val==0xAA)||(val==0x55)) LOG_MSG("TODO implement IBM-style direction switch");
}
void CDirectLPT::Timer2(void)
{

}



#endif
#endif

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


#include "dosbox.h"

#if C_DIRECTLPT

/* Windows version */
#if defined (WIN32)

#include "parport.h"
#include "../../libs/porttalk/porttalk.h"
#include "directlpt_win32.h"
#include "callback.h"
#include <SDL.h> 
#include "setup.h"

CDirectLPT::CDirectLPT (Bitu nr, Bit8u initIrq, CommandLine* cmd)
                              :CParallel (cmd, nr, initIrq) {
	InstallationSuccessful = false;
	interruptflag=true; // interrupt disabled
	realbaseaddress = 0x378;

	std::string str;
	if(cmd->FindStringBegin("realbase:",str,false)) {
		if(sscanf(str.c_str(), "%x",&realbaseaddress)!=1) {
			LOG_MSG("parallel%d: Invalid realbase parameter.",nr);
			return;
		} 
	}

	if(realbaseaddress>=0x10000) {
		LOG_MSG("Error: Invalid base address.");
		return;
	}
	
	if(!initPorttalk()) {
		LOG_MSG("Error: could not open PortTalk driver.");
		return;
	}
	// make sure the user doesn't touch critical I/O-ports
	if((realbaseaddress<0x100) || (realbaseaddress&0x3) ||		// sanity + mainboard res.
		((realbaseaddress>=0x1f0)&&(realbaseaddress<=0x1f7)) ||	// prim. HDD controller
		((realbaseaddress>=0x170)&&(realbaseaddress<=0x177)) ||	// sek. HDD controller
		((realbaseaddress>=0x3f0)&&(realbaseaddress<=0x3f7)) ||	// floppy + prim. HDD
		((realbaseaddress>=0x370)&&(realbaseaddress<=0x377))) {	// sek. hdd
		LOG_MSG("Parallel Port: Invalid base address.");
		return;
	}
	/*	
	if(realbase!=0x378 && realbase!=0x278 && realbase != 0x3bc)
	{
		// TODO PCI ECP ports can be on funny I/O-port-addresses
		LOG_MSG("Parallel Port: Invalid base address.");
		return;
	}*/

	// 0x3bc cannot be a ECP port
	isECP= ((realbaseaddress&0x7)==0);
		
	// add the standard parallel port registers
	addIOPermission((Bit16u)realbaseaddress);
	addIOPermission((Bit16u)realbaseaddress+1);
	addIOPermission((Bit16u)realbaseaddress+2);
	
	// if it could be a ECP port: make the extended control register accessible
	if(isECP)addIOPermission((Bit16u)realbaseaddress+0x402);
	
	// bail out if porttalk fails
	if(!setPermissionList())
	{
		LOG_MSG("ERROR SET PERMLIST");
		return;
	}
	if(isECP) {
		// check if there is a ECP port (try to set bidir)
		originalECPControlReg = inportb(realbaseaddress+0x402);
		Bit8u new_bidir = originalECPControlReg&0x1F;
		new_bidir|=0x20;

		outportb(realbaseaddress+0x402,new_bidir);
		if(inportb(realbaseaddress+0x402)!=new_bidir) {
			// this is not a ECP port
			outportb(realbaseaddress+0x402,originalECPControlReg);
			isECP=false;
		}
	}
	// check if there is a parallel port at all: the autofeed bit
	Bit8u controlreg=inportb(realbaseaddress+2);
	outportb(realbaseaddress+2,controlreg|2);
	if(!(inportb(realbaseaddress+2)&0x2))
	{
		LOG_MSG("No parallel port detected at 0x%x!",realbaseaddress);
		// cannot remember 1
		return;
	}
	
	// check 0
	outportb(realbaseaddress+2,controlreg & ~2);
	if(inportb(realbaseaddress+2)&0x2)
	{
		LOG_MSG("No parallel port detected at 0x%x!",realbaseaddress);
		// cannot remember 0
		return;
	}
	outportb(realbaseaddress+2,controlreg);
	
	if(isECP) LOG_MSG("The port at 0x%x was detected as ECP port.",realbaseaddress);
	else LOG_MSG("The port at 0x%x is not a ECP port.",realbaseaddress);
	
	/*
	// bidir test
	outportb(realbase+2,0x20);
	for(int i = 0; i < 256; i++) {
		outportb(realbase, i);
		if(inportb(realbase)!=i) LOG_MSG("NOT %x", i);
	}
	*/

	// go for it
	ack_polarity=false;
	initialize();

	InstallationSuccessful = true;
	//LOG_MSG("InstSuccess");
}

CDirectLPT::~CDirectLPT () {
	if(InstallationSuccessful && isECP)
		outportb(realbaseaddress+0x402,originalECPControlReg);
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
		return false;
	}
	// error
	if(sr&0x20)
	{
		LOG_MSG("putchar: paper out");
		return false;
	}
	if((sr&0x08)==0)
	{
		LOG_MSG("putchar: printer error");
		return false;
	}
	// busy
	Bitu timeout = 10000;
	Bitu time = timeout+SDL_GetTicks();

	for(int i = 0; i < 150; i++) {
		// do NOT run into callback_idle unless we have to (speeds things up)
		sr=Read_SR();
		if(sr&0x80) break;
	}
	
	while(((sr&0x80)==0)&&(SDL_GetTicks()<time)) {
		// wait for the printer to get ready
		CALLBACK_Idle();
		sr=Read_SR();
	}
	if(SDL_GetTicks()>=time) {
		LOG_MSG("putchar: busy timeout");
		return false;
	}
	// strobe data out
	Write_PR(val);
	
	// I hope this creates a sufficient long pulse...
	// (I/O-Bus at 7.15 MHz will give some delay)
	
	for(int i = 0; i < 8; i++) Write_CON(0xD5); // strobe on
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
#if PARALLEL_DEBUG
	log_par(dbg_putchar,"putchar  0x%2x",val);
	if(dbg_plainputchar) fprintf(debugfp,"%c",val);
#endif

	return true;
}
Bitu CDirectLPT::Read_PR()
{
	return inportb(realbaseaddress);
}
Bitu CDirectLPT::Read_COM()
{
	Bit8u retval=inportb(realbaseaddress+2);
	if(!interruptflag)// interrupt activated
	retval&=~0x10;
	return retval;
}
Bitu CDirectLPT::Read_SR()
{
	return inportb(realbaseaddress+1);
}

void CDirectLPT::Write_PR(Bitu val)
{
	//LOG_MSG("%c,%x",(Bit8u)val,val);
	outportb(realbaseaddress,val);
}
void CDirectLPT::Write_CON(Bitu val)
{
	//do not activate interrupt
	interruptflag = (val&0x10)!=0;
	outportb(realbaseaddress+2,val|0x10);
}
void CDirectLPT::Write_IOSEL(Bitu val)
{
	outportb(realbaseaddress+1,val);
}
void CDirectLPT::Timer2(void)
{

}



#endif
#endif

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
#include "filelpt.h"
#include "callback.h"
#include <SDL.h>

CFileLPT::CFileLPT (Bitu nr, Bit8u initIrq, const char* path, CommandLine* cmd)
                              :CParallel (cmd, nr,initIrq) {
	InstallationSuccessful = false;
	InstallationSuccessful = true;
	//LOG_MSG("InstSuccess");
}

CFileLPT::~CFileLPT () {
	// close file
}

bool CFileLPT::Putchar(Bit8u val)
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

#if PARALLEL_DEBUG
	log_par(dbg_putchar,"putchar  0x%2x",val);
	if(dbg_plainputchar) fprintf(debugfp,"%c",val);
#endif
	return true;
}
Bitu CFileLPT::Read_PR()
{
	return 0;
}
Bitu CFileLPT::Read_COM()
{
	return 0;
}
Bitu CFileLPT::Read_SR()
{
	return 0;
}

void CFileLPT::Write_PR(Bitu val) {
	datareg = (Bit8u)val;
}
void CFileLPT::Write_CON(Bitu val) {

}
void CFileLPT::Write_IOSEL(Bitu val) {

}
void CFileLPT::Timer2(void)
{

}

#endif
#endif

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

#include <string.h>
#include <ctype.h>

#include "dosbox.h"

#include "support.h"
#include "inout.h"
#include "pic.h"
#include "setup.h"
#include "timer.h"
#include "bios.h"					// SetLPTPorts(..)
#include "hardware.h"				// OpenCaptureFile

#include "parport.h"
#include "directlpt_win32.h"
#include "directlpt_linux.h"
#include "printer_redir.h"
#include "filelpt.h"
#include "dos_inc.h"

bool device_LPT::Read(Bit8u * data,Bit16u * size) {
	*size=0;
	LOG(LOG_DOSMISC,LOG_NORMAL)("LPTDEVICE:Read called");
	return true;
}


bool device_LPT::Write(Bit8u * data,Bit16u * size) {
	for (Bit16u i=0; i<*size; i++)
	{
		if(!pportclass->Putchar(data[i])) return false;
	}
	return true;
}

bool device_LPT::Seek(Bit32u * pos,Bit32u type) {
	*pos = 0;
	return true;
}

bool device_LPT::Close() {
	return false;
}

Bit16u device_LPT::GetInformation(void) {
	return 0x80A0;
};
const char* lptname[]={"LPT1","LPT2","LPT3"};
device_LPT::device_LPT(Bit8u num, class CParallel* pp) {
	pportclass = pp;
	SetName(lptname[num]);
	this->num = num;
}

device_LPT::~device_LPT() {
	LOG_MSG("del");
}

static Bitu PARALLEL_Read (Bitu port, Bitu iolen) {
	for(Bitu i = 0; i < 3; i++) {
		if(parallel_baseaddr[i]==(port&0xfffc) && (parallelPortObjects[i]!=0)) {
			Bitu retval=0xff;
			switch (port & 0x7) {
				case 0:
					retval = parallelPortObjects[i]->Read_PR();
					break;
				case 1:
					retval = parallelPortObjects[i]->Read_SR();
					break;
				case 2:
					retval = parallelPortObjects[i]->Read_COM();
					break;
			}

#if PARALLEL_DEBUG
			const char* const dbgtext[]= {"DAT","STA","COM","???"};
			parallelPortObjects[i]->log_par(parallelPortObjects[i]->dbg_cregs,
				"read  0x%2x from %s.",retval,dbgtext[port&3]);
#endif
			return retval;	
		}
	}
	return 0xff;
}

static void PARALLEL_Write (Bitu port, Bitu val, Bitu) {
	for(Bitu i = 0; i < 4; i++) {
		if(parallel_baseaddr[i]==(port&0xfffc) && parallelPortObjects[i]) {
#if PARALLEL_DEBUG
			const char* const dbgtext[]={"DAT","IOS","CON","???"};
			parallelPortObjects[i]->log_par(parallelPortObjects[i]->dbg_cregs,
				"write 0x%2x to %s.",val,dbgtext[port&3]);
			if(parallelPortObjects[i]->dbg_plaindr &&!(port & 0x3)) {
				fprintf(parallelPortObjects[i]->debugfp,"%c",val);
			}
#endif
			switch (port & 0x3) {
				case 0:
					parallelPortObjects[i]->Write_PR (val);
					return;
				case 1:
					parallelPortObjects[i]->Write_IOSEL (val);
					return;
				case 2:
					parallelPortObjects[i]->Write_CON (val);
					return;
			}
		}
	}
}

//The Functions

void CParallel::Timer (void) {
	Timer2();
}

#if PARALLEL_DEBUG
#include <stdarg.h>
void CParallel::log_par(bool active, char const* format,...) {
	if(active) {
		// copied from DEBUG_SHOWMSG
		char buf[512];
		buf[0]=0;
		sprintf(buf,"%12.3f ",PIC_FullIndex());
		va_list msg;
		va_start(msg,format);
		vsprintf(buf+strlen(buf),format,msg);
		va_end(msg);
		// Add newline if not present
		Bitu len=strlen(buf);
		if(buf[len-1]!='\n') strcat(buf,"\r\n");
		fputs(buf,debugfp);
	}
}
#endif

// Initialisation
CParallel::CParallel(CommandLine* cmd, Bitu portnr, Bit8u initirq) {
	base = parallel_baseaddr[portnr];
	irq = initirq;

#if PARALLEL_DEBUG
	dbg_data	= cmd->FindExist("dbgdata", false);
	dbg_putchar = cmd->FindExist("dbgput", false);
	dbg_cregs	= cmd->FindExist("dbgregs", false);
	dbg_plainputchar = cmd->FindExist("dbgputplain", false);
	dbg_plaindr = cmd->FindExist("dbgdataplain", false);
	
	if(cmd->FindExist("dbgall", false)) {
		dbg_data= 
		dbg_putchar=
		dbg_cregs=true;
		dbg_plainputchar=dbg_plaindr=false;
	}

	if(dbg_data||dbg_putchar||dbg_cregs||dbg_plainputchar||dbg_plaindr)
		debugfp=OpenCaptureFile("parlog",".parlog.txt");
	else debugfp=0;

	if(debugfp == 0) {
		dbg_data= 
		dbg_putchar=dbg_plainputchar=
		dbg_cregs=false;
	} else {
		std::string cleft;
		cmd->GetStringRemain(cleft);

		log_par(true,"Parallel%d: BASE %xh, initstring \"%s\"\r\n\r\n",
			portnr+1,base,cleft.c_str());
	}
#endif

	for (Bitu i = 0; i < 3; i++) {
		WriteHandler[i].Install (i + base, PARALLEL_Write, IO_MB);
		ReadHandler[i].Install (i + base, PARALLEL_Read, IO_MB);
	}
	mydosdevice=new device_LPT(portnr, this);
	DOS_AddDevice(mydosdevice);
};

CParallel::~CParallel(void) {
	if(mydosdevice) DOS_DelDevice(mydosdevice);
};

Bit8u CParallel::getPrinterStatus()
{
	/*	7      not busy
		6      acknowledge
		5      out of paper
		4      selected
		3      I/O error
		2-1    unused
		0      timeout  */
	Bit8u statusreg=Read_SR();

	LOG_MSG("get printer status: %x",statusreg);
	statusreg^=0x48;
	return statusreg&~0x7;
}

#include "callback.h"

void RunIdleTime(Bitu milliseconds)
{
	Bitu time=SDL_GetTicks()+milliseconds;
	while(SDL_GetTicks()<time)
		CALLBACK_Idle();
}

void CParallel::initialize()
{
	Write_IOSEL(0x55); // output mode
	Write_CON(0xD0); // init on
	Write_PR(0);
	RunIdleTime(10);
	Write_CON(0xD4); // init off
	RunIdleTime(500);
	LOG_MSG("printer init data");
}



CParallel* parallelPortObjects[3];
class PARPORTS:public Module_base {
public:
	
	PARPORTS (Section * configuration):Module_base (configuration) {

#if C_PRINTER
		bool printer_used = false;
#endif

		// default ports & interrupts
		Bit8u defaultirq[] = { 7, 5, 12};
		Bit16u biosParameter[3] = { 0, 0, 0};
		Section_prop *section = static_cast <Section_prop*>(configuration);
		
		char pname[]="parallelx";
		// iterate through all 3 lpt ports
		for (Bitu i = 0; i < 3; i++) {
			pname[8] = '1' + i;
			CommandLine cmd(0,section->Get_string(pname));

			std::string str;
			cmd.FindCommand(1,str);
#ifdef C_DIRECTLPT			
			if(str=="reallpt") {	LOG_MSG("reallpt");
				CDirectLPT* cdlpt= new CDirectLPT(i, defaultirq[i],&cmd);
				if(cdlpt->InstallationSuccessful)
					parallelPortObjects[i]=cdlpt;
				else {
					delete cdlpt;
					parallelPortObjects[i]=0;
				}
			}
			else 
#endif
	/*		else if(!str.compare("filelpt")) {
				const char* path;
				if(cmd->FindStringBegin("file:",str,false)) {
					path = str.c_str();
				 
				} else continue;

				CFileLPT* cflpt= new CFileLPT(parallelReadHandlers[i], 
						parallelWriteHandlers[i],parallelTimerHandlers[i],
						baseAddress,defaultirq[i], i,path);
				if(cflpt->InstallationSuccessful)
					parallelPortObjects[i]=cflpt;
				else {
					delete cflpt;
					parallelPortObjects[i]=0;
				}
			}
			else */
#if C_PRINTER
			if(str=="printer") {
				if(printer_used) {
					
				}; // only one parallel port with printer
				CPrinterRedir* cprd = new CPrinterRedir(i,defaultirq[i],&cmd);
				if(cprd->InstallationSuccessful) {
					parallelPortObjects[i]=cprd;
					printer_used=true;
				} else {
					LOG_MSG("Error: printer is not enabled.");
					delete cprd;
					parallelPortObjects[i]=0;
				}
			} else
#endif				
			if(str=="disabled") {
				parallelPortObjects[i] = 0;
			} else {
				LOG_MSG ("Invalid type for LPT%d.", i + 1);
				parallelPortObjects[i] = 0;
			}
			if(parallelPortObjects[i]) biosParameter[i] = parallel_baseaddr[i];
		} // for lpt 1-3
		BIOS_SetLPTPorts (biosParameter);
	}

	~PARPORTS () {
		for (Bitu i = 0; i < 3; i++)
			if (parallelPortObjects[i]) {
				delete parallelPortObjects[i];
				parallelPortObjects[i] = 0;
			}
	}
};

static PARPORTS *testParallelPortsBaseclass;

void PARALLEL_Destroy (Section * sec) {
	delete testParallelPortsBaseclass;
	testParallelPortsBaseclass = NULL;
}

void PARALLEL_Init (Section * sec) {
	// should never happen
	if (testParallelPortsBaseclass) delete testParallelPortsBaseclass;
	testParallelPortsBaseclass = new PARPORTS (sec);
	sec->AddDestroyFunction (&PARALLEL_Destroy, true);
		LOG_MSG("printer init");
}

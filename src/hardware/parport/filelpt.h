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

// include guard
#ifndef DOSBOX_FILELPT_H
#define DOSBOX_FILELPT_H

#include "dosbox.h"
#include "parport.h"

class CFileLPT : public CParallel {
public:
	CFileLPT(
			Bitu nr,
			Bit8u initIrq,
            const char* path,
			CommandLine* cmd
            );
	

	~CFileLPT();
	
	bool InstallationSuccessful;	// check after constructing. If
									// something was wrong, delete it right away.
	bool ack_polarity;

	Bitu Read_PR();
	Bitu Read_COM();
	Bitu Read_SR();

	Bit8u datareg;

	void Write_PR(Bitu);
	void Write_CON(Bitu);
	void Write_IOSEL(Bitu);
	bool Putchar(Bit8u);

	void Timer2(void);
};

#endif	// include guard

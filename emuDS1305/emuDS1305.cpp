// emuDS1305.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <unordered_map>
using std::unordered_map;

#include <chrono>
using std::chrono::system_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;

#include <iostream>
using std::cout;
using std::endl;
using std::hex;

#include "emuDS1305.h"

// Seconds in a day
#define ONE_DAY 86400

const unsigned char BCDIncrementTable[] =
{
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10,
	0, 0, 0, 0, 0, 0,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20,
	0, 0, 0, 0, 0, 0,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x30,
	0, 0, 0, 0, 0, 0,
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x40,
	0, 0, 0, 0, 0, 0,
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x50,
	0, 0, 0, 0, 0, 0,
	0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x00
};

const unsigned char Hour12IncrementTable[] =
{
	0,    0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x50,
	0, 0, 0, 0, 0, 0,
	0x51, 0x72, 0x41,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70,
	0, 0, 0, 0, 0, 0,
	0x71, 0x52, 0x61
};

const unsigned char Hour24IncrementTable[] =
{
	0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10,
	0, 0, 0, 0, 0, 0,
	0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20,
	0, 0, 0, 0, 0, 0,
	0x21, 0x22, 0x23, 0x00
};

const unsigned char IntToBCDTable[] =
{
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99
};

DS1305::DS1305()
{
	// Initialize registers and RAM
	seconds.byte = 0;
	minutes.byte = 0;
	hours.byte = 0;
	day.byte = 0;
	date.byte = 0;
	month.byte = 0;
	year.byte = 0;
	alarm0seconds.byte = 0;
	alarm0minutes.byte = 0;
	alarm0hours.byte = 0;
	alarm0day.byte = 0;
	alarm1seconds.byte = 0;
	alarm1minutes.byte = 0;
	alarm1hours.byte = 0;
	alarm1day.byte = 0;
	control.byte = 0xC0; // nEOSC = 1 on powerup, WP undefined so assume 1
	status.byte = 0;
	tricklecharge.byte = 0;
	memset(RAM, 0x00, sizeof(RAM));

	// Initialize IO pins
	INT0 = 1;
	INT1 = 1;
	SERMODE = 0;
	CE = 0;
	SCLK = 0;
	SDI = 0;
	SDO = 0;
	PF = 1;
}


DS1305::~DS1305()
{
}

void DS1305::Execute()
{
	if (!control.ctrl.nEOSC)
	{
		if (UpdateTime())
			CheckAlarms();
	}

	if (SERMODE)
		CheckComm();
	// 3 Wire communication not implemented
}

bool DS1305::UpdateTime()
{
	// WARNING:  Countdown chain is not reset on a write to the seconds register
	// This means seconds can increment on the next Execute() call after a write to seconds
	// (Probbaly not a big deal but this is different than actual HW behavior)
	static auto start = system_clock::now();
	auto now = system_clock::now();
	auto diff = duration_cast<milliseconds>(now - start).count();

	// check if one second has passed
	if (diff >= 1000)
	{
		start = now;
		if (IncrementSeconds())
		{
			if (IncrementMinutes())
			{
				if (IncrementHours())
				{
					IncrementDay();
					IncrementDate();
				}
			}
		}
		// Time has been updated
		return true;
	}

	// Time has not been updated
	return false;
}


void DS1305::CheckAlarms()
{
	if (CheckAlarm0())
	{
		status.stat.IRQF0 = 1;
		if (control.ctrl.AIE0)
			INT0 = 0;
	}

	if (CheckAlarm1())
	{
		status.stat.IRQF1 = 1;
		if (control.ctrl.AIE1)
		{
			if (control.ctrl.INTCN)
				INT1 = 0;
			else
				INT0 = 0;
		}
	}
}


void DS1305::CheckComm()
{
	static unsigned char counter = 0;
	static unsigned char inputBuff = 0x00;
	static unsigned char outputBuff = 0x00;
	static unsigned char addressPtr = 0xFF;
	static unsigned char *registerPtr = 0;
	static unsigned char prevclk = 0;
	static unsigned char clkpol = false;
	static bool clkpol_set = false;
	static bool writecmd = false;	// read = false, write = true

	const static unordered_map<unsigned char, unsigned char*> memmap
	{
		{ 0x00, &seconds.byte },{ 0x01, &minutes.byte },{ 0x02, &hours.byte },{ 0x03, &day.byte },{ 0x04, &date.byte },{ 0x05, &month.byte },{ 0x06, &year.byte },
		{ 0x07, &alarm0seconds.byte },{ 0x08, &alarm0minutes.byte },{ 0x09, &alarm0hours.byte },{ 0x0A, &alarm0day.byte },
		{ 0x0B, &alarm1seconds.byte },{ 0x0C, &alarm1minutes.byte },{ 0x0D, &alarm1hours.byte },{ 0x0E, &alarm1day.byte },
		{ 0x0F, &control.byte },{ 0x10,&status.byte },{ 0x11,&tricklecharge.byte }
	};

	if (!CE)
	{
		// Reset
		counter = 0;
		inputBuff = 0x00;
		outputBuff = 0x00;
		addressPtr = 0xFF;
		prevclk = 0;
		clkpol_set = false;
		writecmd = false;
		return;
	}

	// Set clock polarity if we're starting a new data transfer
	if (!clkpol_set)
	{
		clkpol = SCLK;
		clkpol_set = true;
		prevclk = SCLK;
		return;
	}

	// First clock edge
	if ((SCLK != prevclk) && (SCLK != clkpol))
	{
		// Shift data out on internal strobe edge
		SDO = (outputBuff >> (7 - counter)) & 0x01;
		// Clear IRQFs if counter is 0
		// This means we're starting a new data transfer so we're safe to clear the flags.
		// Originally did this in the counter == 8 block
		// but this created a bug where the address pointer was incremented in preparation
		// for the next data transfer (burst mode) and the flags were being cleared
		// inadvertently - e.g., reading/writing Alarm0 Day register incremented address pointer
		// into Alarm1 Seconds register, clearing IRQF1 even when Alarm1 Seconds register was
		// not read/written.
		// Doing it here ensures a new data transfer is being initiated, so we're safe to check
		// the address pointer and clear the flags.
		if (counter == 0 && addressPtr != 0xFF)
		{
			// If addressPtr is in Alarm0, clear IRQF0
			if ((addressPtr >= 0x07) && (addressPtr <= 0x0A))
			{
				status.byte &= 0xFE;
				if ((control.ctrl.INTCN) || (!control.ctrl.INTCN && !status.stat.IRQF1))
					INT0 = 1;
			}
			// If addressPtr is in Alarm1, clear IRQF1
			else if ((addressPtr >= 0x0B) && (addressPtr <= 0x0E))
			{
				status.byte &= 0xFD;
				if (control.ctrl.INTCN)
					INT1 = 1;
				else if (!status.stat.IRQF0)
					INT0 = 1;
			}
		}	
	}
	else if ((SCLK != prevclk) && (SCLK == clkpol))
	{
		if (SDI)
			inputBuff |= (1 << (7 - counter));
		// Increment counter here since we've gone through a full clock cycle
		counter++;
	}
	prevclk = SCLK;

	if (counter == 8)
	{
		// We've transferred a whole byte of data, reset the counter
		counter = 0;

		if (!writecmd && addressPtr != 0xFF)
		{
			// If we're reading, increment the address pointer now
			if (addressPtr == 0x1F)
				addressPtr = 0x00;
			else if (addressPtr == 0x7F)
				addressPtr = 0x20;
			else
				addressPtr++;
		}
		// Set the address pointer on first byte received
		else if (addressPtr == 0xFF)
		{
			writecmd = (inputBuff & 0x80) ? true : false;
			addressPtr = (inputBuff & 0x7F);
			if (writecmd)
			{
				inputBuff = 0x00;
				return;
			}
		}

		// Get the register address pointer points to
		auto search = memmap.find(addressPtr);
		if (writecmd)
		{
			if (search != memmap.end())
				*(search->second) = inputBuff;
			else
			{
				// 0x20 or above, in RAM
				if (addressPtr >= 0x20)
					RAM[addressPtr - 0x20] = inputBuff;
				// All other values in reserved section - write has no effect
			}
		}
		else
		{
			if (search != memmap.end())
				outputBuff = *(search->second);
			else
			{
				// 0x20 or above, in RAM
				if (addressPtr >= 0x2F)
					outputBuff = RAM[addressPtr - 0x20];
				// All other values in reserved section - read as 0x00
				else
					outputBuff = 0x00;
			}
		}

		// If we're writing, increment the address pointer now
		if (writecmd)
		{
			if (addressPtr == 0x1F)
				addressPtr = 0x00;
			else if (addressPtr == 0x7F)
				addressPtr = 0x20;
			else
				addressPtr++;
		}	

		// Reset the buffer
		inputBuff = 0x00;
	}
}

bool DS1305::IncrementSeconds()
{
	seconds.byte = BCDIncrementTable[seconds.byte];

	if (seconds.byte == 0)
		return true;

	return false;
}


bool DS1305::IncrementMinutes()
{
	minutes.byte = BCDIncrementTable[minutes.byte];

	if (minutes.byte == 0)
		return true;

	return false;
}


bool DS1305::IncrementHours()
{
	// 12 hour mode
	if (hours.time.hour_mode)
	{
		// Don't pass in the hours mode bit
		hours.byte = Hour12IncrementTable[hours.byte & 0x3F];
		// If 12AM, increment day
		if (hours.byte == 0x52)
			return true;
	}
	// 24 hour mode
	else
	{
		hours.byte = Hour24IncrementTable[hours.byte];
		// If 12AM, increment day
		if (hours.byte == 0x00)
			return true;
	}

	return false;
}


void DS1305::IncrementDay()
{
	if (day.byte == 7)
		day.byte = 1;
	else
		day.byte++;
}


void DS1305::IncrementDate()
{
	static time_t tmp = time(0);
	static tm regtime;
	localtime_s(&regtime, &tmp);

	regtime.tm_year = (int)((year.byte & 0xF0) >> 4) * 10 + (int)(year.byte & 0x0F) + 100;
	regtime.tm_mon = (int)((month.byte & 0x30) >> 4) * 10 + (int)(month.byte & 0x0F) - 1;
	regtime.tm_wday = 0;
	regtime.tm_mday = (int)((date.byte & 0x30) >> 4) * 10 + (int)(date.byte & 0x0F);
	regtime.tm_hour = 0;
	regtime.tm_min = 0;
	regtime.tm_sec = 0;
	// Set DST flag to 0 to force hour to rollover at all times of year
	regtime.tm_isdst = 0;

	auto regtime_t = mktime(&regtime);
	regtime_t += ONE_DAY;

	tm newtime;
	localtime_s(&newtime, &regtime_t);

	// Set date/month/year
	date.byte = IntToBCDTable[newtime.tm_mday];
	month.byte = IntToBCDTable[newtime.tm_mon + 1];
	year.byte = IntToBCDTable[newtime.tm_year - 100];
}

bool DS1305::CheckAlarm0()
{
	// I'm positive there's a better way to do this
	if (alarm0seconds.time.M & alarm0minutes.time.M & alarm0hours.time.M & alarm0day.time.M)
		return true;

	if (!alarm0seconds.time.M & alarm0minutes.time.M & alarm0hours.time.M & alarm0day.time.M)
	{
		if ((alarm0seconds.byte & 0x7F) == (seconds.byte & 0x7F))
			return true;
	}

	if (!alarm0seconds.time.M & !alarm0minutes.time.M & alarm0hours.time.M & alarm0day.time.M)
	{
		if (((alarm0seconds.byte & 0x7F) == (seconds.byte & 0x7F)) && \
			((alarm0minutes.byte & 0x7F) == (minutes.byte & 0x7F)))
			return true;
	}

	if (!alarm0seconds.time.M & !alarm0minutes.time.M & !alarm0hours.time.M & alarm0day.time.M)
	{
		if (((alarm0seconds.byte & 0x7F) == (seconds.byte & 0x7F)) && \
			((alarm0minutes.byte & 0x7F) == (minutes.byte & 0x7F)) && \
			((alarm0hours.byte & 0x7F) == (hours.byte & 0x7F)))
			return true;
	}

	if (!alarm0seconds.time.M & !alarm0minutes.time.M & !alarm0hours.time.M & !alarm0day.time.M)
	{
		if (((alarm0seconds.byte & 0x7F) == (seconds.byte & 0x7F)) && \
			((alarm0minutes.byte & 0x7F) == (minutes.byte & 0x7F)) && \
			((alarm0hours.byte & 0x7F) == (hours.byte & 0x7F)) && \
			((alarm0day.byte & 0x7F) == (day.byte & 0x7F)))
			return true;
	}

	return false;
}


bool DS1305::CheckAlarm1()
{
	// I'm positive there's a better way to do this
	if (alarm1seconds.time.M & alarm1minutes.time.M & alarm1hours.time.M & alarm1day.time.M)
		return true;

	if (!alarm1seconds.time.M & alarm1minutes.time.M & alarm1hours.time.M & alarm1day.time.M)
	{
		if ((alarm1seconds.byte & 0x7F) == (seconds.byte & 0x7F))
			return true;
	}

	if (!alarm1seconds.time.M & !alarm1minutes.time.M & alarm1hours.time.M & alarm1day.time.M)
	{
		if (((alarm1seconds.byte & 0x7F) == (seconds.byte & 0x7F)) && \
			((alarm1minutes.byte & 0x7F) == (minutes.byte & 0x7F)))
			return true;
	}

	if (!alarm1seconds.time.M & !alarm1minutes.time.M & !alarm1hours.time.M & alarm1day.time.M)
	{
		if (((alarm1seconds.byte & 0x7F) == (seconds.byte & 0x7F)) && \
			((alarm1minutes.byte & 0x7F) == (minutes.byte & 0x7F)) && \
			((alarm1hours.byte & 0x7F) == (hours.byte & 0x7F)))
			return true;
	}

	if (!alarm1seconds.time.M & !alarm1minutes.time.M & !alarm1hours.time.M & !alarm1day.time.M)
	{
		if (((alarm1seconds.byte & 0x7F) == (seconds.byte & 0x7F)) && \
			((alarm1minutes.byte & 0x7F) == (minutes.byte & 0x7F)) && \
			((alarm1hours.byte & 0x7F) == (hours.byte & 0x7F)) && \
			((alarm1day.byte & 0x7F) == (day.byte & 0x7F)))
			return true;
	}

	return false;
}

void DS1305::Debug()
{
	cout << "-- SPECIAL REGISTERS --" << endl;
	cout << "Control: " << hex << (int)control.byte << endl;
	cout << "Status: " << hex << (int)status.byte << endl;
	cout << "-- TIME REGISTERS --" << endl;
	cout << "Seconds: " << hex << (int)seconds.byte << endl;
	cout << "Minutes: " << hex << (int)minutes.byte << endl;
	cout << "Hours: " << hex << (int)hours.byte << endl;
	cout << "Day: " << hex << (int)day.byte << endl;
	cout << "Date: " << hex << (int)date.byte << endl;
	cout << "Month: " << hex << (int)month.byte << endl;
	cout << "Year: " << hex << (int)year.byte << endl;
	cout << "-- ALARM0 REGISTERS --" << endl;
	cout << "Seconds: " << hex << (int)alarm0seconds.byte << endl;
	cout << "Minutes: " << hex << (int)alarm0minutes.byte << endl;
	cout << "Hours: " << hex << (int)alarm0hours.byte << endl;
	cout << "Day: " << hex << (int)alarm0day.byte << endl;
	cout << "-- ALARM1 REGISTERS --" << endl;
	cout << "Seconds: " << hex << (int)alarm1seconds.byte << endl;
	cout << "Minutes: " << hex << (int)alarm1minutes.byte << endl;
	cout << "Hours: " << hex << (int)alarm1hours.byte << endl;
	cout << "Day: " << hex << (int)alarm1day.byte << endl;
	cout << "-- RAM REGISTERS --" << endl;
	cout << "RAM[0]: " << hex << (int)RAM[0] << endl;
	cout << "RAM[1]: " << hex << (int)RAM[1] << endl;
	cout << "RAM[94]: " << hex << (int)RAM[94] << endl;
	cout << "RAM[95]: " << hex << (int)RAM[95] << endl;
	cout << endl;
}
#pragma once

#ifdef EMUDS1305_EXPORTS
#define EMUDS1305_API __declspec(dllexport)
#else
#define EMUDS1305_API __declspec(dllimport)
#endif

#include <vector>
using std::vector;

// Generic register data type
typedef union
{
	unsigned char byte;
	// Control register bits
	struct
	{
		unsigned	AIE0 : 1,
			AIE1 : 1,
			INTCN : 1,
			unused : 3,
			WP : 1,
			nEOSC : 1;
	}ctrl;
	// Status register bits
	struct
	{
		unsigned	IRQF0 : 1,
			IRQF1 : 1,
			unused : 6;
	}stat;
	// General time bits
	struct
	{
		unsigned	unused : 5,
			am_pm : 1,
			hour_mode : 1,
			M : 1;
	}time;
} Register;

class DS1305
{
public:
	EMUDS1305_API DS1305();
	EMUDS1305_API ~DS1305();

	// I/O pins
	unsigned char INT0;
	unsigned char INT1;
	unsigned char SERMODE;
	unsigned char CE;
	unsigned char SCLK;
	unsigned char SDI;
	unsigned char SDO;
	unsigned char PF;

	EMUDS1305_API void Execute();
	EMUDS1305_API void Debug();

private:
	// Registers

	// Current time/date
	Register seconds;
	Register minutes;
	Register hours;
	Register day;
	Register date;
	Register month;
	Register year;
	// Alarm0
	Register alarm0seconds;
	Register alarm0minutes;
	Register alarm0hours;
	Register alarm0day;
	// Alarm1
	Register alarm1seconds;
	Register alarm1minutes;
	Register alarm1hours;
	Register alarm1day;
	// Special purpose registers
	Register control;
	Register status;
	Register tricklecharge;
	// 96 bytes NV RAM
	unsigned char RAM[96];

	bool UpdateTime();
	void CheckAlarms();
	void CheckComm();
	bool IncrementSeconds();
	bool IncrementMinutes();
	bool IncrementHours();
	void IncrementDay();
	void IncrementDate();
	bool CheckAlarm0();
	bool CheckAlarm1();
};


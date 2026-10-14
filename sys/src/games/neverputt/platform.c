#include "port.h"

static struct {
	int key;
	char *name;
} keynames[] = {
	{Kesc,"Escape"}, {'\n',"Return"}, {'\t',"Tab"}, {' ',"Space"},
	{Kleft,"Left"}, {Kright,"Right"}, {Kup,"Up"}, {Kdown,"Down"},
	{Kshift,"Left Shift"}, {Kctl,"Left Ctrl"}, {Kalt,"Left Alt"},
	{KF|1,"F1"}, {KF|2,"F2"}, {KF|3,"F3"}, {KF|4,"F4"},
	{KF|5,"F5"}, {KF|6,"F6"}, {KF|7,"F7"}, {KF|8,"F8"},
	{KF|9,"F9"}, {KF|10,"F10"}, {KF|11,"F11"}, {KF|12,"F12"},
};

int
key_code(const char *name)
{
	int i;

	for(i=0; i<nelem(keynames); i++)
		if(cistrcmp(name,keynames[i].name) == 0)
			return keynames[i].key;
	if(name[0] && name[1] == 0)
		return tolower(name[0]);
	return 0;
}

const char*
key_name(int key)
{
	static char buf[UTFmax+1];
	Rune r;
	int i;

	for(i=0; i<nelem(keynames); i++)
		if(key == keynames[i].key)
			return keynames[i].name;
	r = key;
	buf[runetochar(buf,&r)] = 0;
	return buf;
}

const char *display_name(int n) { return n == 0 ? "rio" : nil; }
int display_count(void) { return 1; }

#ifndef BOOTMENU_H_
#define BOOTMENU_H_

void pickmenuslot();
char mainmenu();
void runbootfrommenu(int select);
void commandfrommenu(char * command, int confirm);
char* completeVersion();
void editmenuoptions();
void presentmenuslots();
int deletemenuslot();
int renamemenuslot();
int reordermenuslot();
int edituserdefinedcommand();
void printnewmenuslot(int pos, int color, char* name);
void getslotfromem(int slotnumber);
void putslottoem(int slotnumber);
char menuslotkey(int slotnumber);
int keytomenuslot(char keypress);

#endif
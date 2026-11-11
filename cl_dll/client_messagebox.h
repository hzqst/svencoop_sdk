#ifndef CLIENT_MESSAGEBOX_H
#define CLIENT_MESSAGEBOX_H

// MessageBox constants with SDL-compatible icon flags
#define MSGBOX_ICONERROR		0x10
#define MSGBOX_ICONWARNING		0x20
#define MSGBOX_ICONINFORMATION	0x40

#define MSGBOX_OK				0
#define MSGBOX_OKCANCEL			1
#define MSGBOX_ABORTRETRYIGNORE	2
#define MSGBOX_YESNOCANCEL		3
#define MSGBOX_YESNO			4
#define MSGBOX_RETRYCANCEL		5

#define MSGBOX_IDOK			1
#define MSGBOX_IDCANCEL		2
#define MSGBOX_IDABORT		3
#define MSGBOX_IDRETRY		4
#define MSGBOX_IDIGNORE		5
#define MSGBOX_IDYES		6
#define MSGBOX_IDNO			7
#define MSGBOX_IDCLOSE		8

int UTIL_MessageBox(const char *pszMessage, const char *pszTitle, int iIcon, int iButtons);

#endif

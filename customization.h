#ifndef CUSTOMIZATION_H
#define CUSTOMIZATION_H

#include "platform-constants.h"

#define NAME "AzireVPN"
#define ORGANIZATION "Netbouncer AB"
#define API_BASE "https://api.azirevpn.com/"
#define USERAGENT NAME "/" VERSION " (" PLATFORM_AGENT ")"
#define STR2DATE(x) QDate::fromString(x, "yyyy-MM-dd 00:00:00")

#endif // CUSTOMIZATION_H

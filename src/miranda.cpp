// Copyright 2013-2016, Durachenko Aleksey V. <durachenko.aleksey@gmail.com>
//                2011, Ruslan Nigmatullin <euroelessar@yandex.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#include "miranda.h"
#include <QDebug>
#include <QFile>
#include <iostream>
#include <QHash>
#include <QTextCodec>
#include <QVariant>
#include <json.h>


// All this typenames are from Miranda sources
typedef quint32 DWORD;
typedef quint16 WORD;
typedef quint8  BYTE;
typedef quint16 WCHAR;
typedef qint8   TCHAR;


const char *DBHEADER_SIGNATURE = "Miranda ICQ DB";
struct DBHeader {
    BYTE signature[16]; // 'Miranda ICQ DB',0,26
    DWORD version;      // as 4 bytes, ie 1.2.3.10=0x0102030a
    // this version is 0x00000700
    DWORD ofsFileEnd;   // offset of the end of the database - place to write
    // new structures
    DWORD slackSpace;   // a counter of the number of bytes that have been
    // wasted so far due to deleting structures and/or
    // re-making them at the end. We should compact when
    // this gets above a threshold
    DWORD contactCount;     // number of contacts in the chain,excluding the user
    DWORD ofsFirstContact;  // offset to first struct DBContact in the chain
    DWORD ofsUser;          // offset to struct DBContact representing the user
    DWORD ofsFirstModuleName;   // offset to first struct DBModuleName in the chain
};


static const DWORD DBCONTACT_SIGNATURE = 0x43DECADEu;
struct DBContact {
    DWORD signature;
    DWORD ofsNext;      // offset to the next contact in the chain. zero if
    // this is the 'user' contact or the last contact
    // in the chain
    DWORD ofsFirstSettings; // offset to the first DBContactSettings in the
    // chain for this contact.
    DWORD eventCount;   // number of events in the chain for this contact
    DWORD ofsFirstEvent;    // offsets to the first and last DBEvent in
    DWORD ofsLastEvent;     // the chain for this contact
    DWORD ofsFirstUnreadEvent;  // offset to the first (chronological) unread event
    // in the chain, 0 if all are read
    DWORD timestampFirstUnread; // timestamp of the event at ofsFirstUnreadEvent
};


enum DBEF {
    DBEF_FIRST =  1,    // this is the first event in the chain;
    // internal only: *do not* use this flag
    DBEF_SENT  =  2,    // this event was sent by the user. If not set this
    // event was received.
    DBEF_READ  =  4,    // event has been read by the user. It does not need
    // to be processed any more except for history.
    DBEF_RTL   =  8,    // event contains the right-to-left aligned text
    DBEF_UTF   = 16     // event contains a text in utf-8
};


enum EVENTTYPE {
    EVENTTYPE_MESSAGE  = 0,
    EVENTTYPE_URL      = 1,
    EVENTTYPE_CONTACTS = 2, // v0.1.2.2+
    EVENTTYPE_ADDED       = 1000,  // v0.1.1.0+: these used to be module-
    EVENTTYPE_AUTHREQUEST = 1001,  // specific codes, hence the module-
    EVENTTYPE_FILE        = 1002,  // specific limit has been raised to 2000
};


static const DWORD DBEVENT_SIGNATURE = 0x45DECADEu;
struct DBEvent {
    DWORD signature;
    DWORD ofsPrev;  // offset to the previous and next events in the
    DWORD ofsNext;  // chain. Chain is sorted chronologically
    DWORD ofsModuleName;    // offset to a DBModuleName struct of the name of
    // the owner of this event
    DWORD timestamp;    // seconds since 00:00:00 01/01/1970
    DWORD flags;        // see m_database.h, db/event/add
    WORD eventType;     // module-defined event type
    DWORD cbBlob;       // number of bytes in the blob
    QByteArray blob;    // the blob. module-defined formatting
};


static const DWORD DBMODULENAME_SIGNATURE = 0x4DDECADEu;
struct DBModuleName {
    DWORD signature;
    DWORD ofsNext;  // offset to the next module name in the chain
    BYTE cbName;    // number of characters in this module name
    QByteArray name;    // name, no nul terminator
};


static const DWORD DBCONTACTSETTINGS_SIGNATURE = 0x53DECADEu;
struct DBContactSettings {
    DWORD signature;
    DWORD ofsNext;          // offset to the next contactsettings in the chain
    DWORD ofsModuleName;    // offset to the DBModuleName of the owner of these
    // settings
    DWORD cbBlob;   // size of the blob in bytes. May be larger than the
    // actual size for reducing the number of moves
    // required using granularity in resizing
    QByteArray blob;    // the blob. a back-to-back sequence of DBSetting
    // structs, the last has cbName=0
};


// DBVARIANT: used by db/contact/getsetting and db/contact/writesetting
enum DBVT {
    DBVT_DELETED    = 0,    // this setting just got deleted, no other values are valid
    DBVT_BYTE       = 1,    // bVal and cVal are valid
    DBVT_WORD       = 2,    // wVal and sVal are valid
    DBVT_DWORD      = 4,    // dVal and lVal are valid
    DBVT_ASCIIZ     = 255,  // pszVal is valid
    DBVT_BLOB       = 254,  // cpbVal and pbVal are valid
    DBVT_UTF8       = 253,  // pszVal is valid
    DBVT_WCHAR      = 252,  // pszVal is valid
    DBVT_TCHAR      = DBVT_WCHAR
};

static const DWORD DBVTF_VARIABLELENGTH = 0x80;
static const DWORD DBVTF_DENYUNICODE    = 0x10000;
typedef struct {
    BYTE type;
    union {
        BYTE bVal;
        char cVal;
        WORD wVal;
        short sVal;
        DWORD dVal;
        long lVal;
        struct {
            union {
                char *pszVal;
                TCHAR *ptszVal;
                WCHAR *pwszVal;
            };
            WORD cchVal;    // only used for db/contact/getsettingstatic
        };
        struct {
            WORD cpbVal;
            BYTE *pbVal;
        };
    };
} DBVARIANT;


inline DWORD ReadDWord(const BYTE *&data)
{
    DWORD a = *(data++);
    a += (*(data++) << 8);
    a += (*(data++) << 16);
    a += (*(data++) << 24);

    return a;
}


inline WORD ReadWord(const BYTE *&data)
{
    WORD a = *(data++);
    a += (*(data++) << 8);

    return a;
}


inline BYTE ReadByte(const BYTE *&data)
{
    return *(data++);
}


inline QByteArray ReadByteArray(const BYTE *&data)
{
    WORD lenth = ReadWord(data);
    QByteArray result((const char *)data, lenth);
    data += lenth;

    return result;
}


static DBHeader ReadDBHeader(const BYTE *data)
{
    DBHeader header;
    for (int i = 0; i < 16; i++) {
        header.signature[i] = ReadByte(data);
    }
    header.version = ReadDWord(data);
    header.ofsFileEnd = ReadDWord(data);
    header.slackSpace = ReadDWord(data);
    header.contactCount = ReadDWord(data);
    header.ofsFirstContact = ReadDWord(data);
    header.ofsUser = ReadDWord(data);
    header.ofsFirstModuleName = ReadDWord(data);

    return header;
}


static DBContact ReadDBContact(const BYTE *data, const BYTE *lastDataAddr)
{
    if ((lastDataAddr - data) < 32) {
        throw QString("invalid data format");
    }

    DBContact contact;
    contact.signature = ReadDWord(data);
    contact.ofsNext = ReadDWord(data);
    contact.ofsFirstSettings = ReadDWord(data);
    contact.eventCount = ReadDWord(data);
    contact.ofsFirstEvent = ReadDWord(data);
    contact.ofsLastEvent = ReadDWord(data);
    contact.ofsFirstUnreadEvent = ReadDWord(data);
    contact.timestampFirstUnread = ReadDWord(data);

    return contact;
}


static DBEvent ReadDBEvent(const BYTE *data, const BYTE *lastDataAddr)
{
    if ((lastDataAddr - data) < 32) {
        throw QString("invalid data format");
    }

    DBEvent event;
    event.signature = ReadDWord(data);
    event.ofsPrev = ReadDWord(data);
    event.ofsNext = ReadDWord(data);
    event.ofsModuleName = ReadDWord(data);
    event.timestamp = ReadDWord(data);
    event.flags = ReadDWord(data);
    event.eventType = ReadWord(data);
    event.cbBlob = ReadDWord(data);
    if (event.cbBlob > (lastDataAddr - data)) {
        throw QString("invalid data format");
    }
    event.blob = QByteArray((const char *)data, event.cbBlob);

    return event;
}


static DBModuleName ReadDBModuleName(const BYTE *data, const BYTE *lastAddr)
{
    if ((lastAddr - data) < 9) {
        throw QString("invalid data format");
    }

    DBModuleName moduleName;
    moduleName.signature = ReadDWord(data);
    moduleName.ofsNext = ReadDWord(data);
    moduleName.cbName = ReadByte(data);
    if (moduleName.cbName > (lastAddr - data)) {
        throw QString("invalid data format");
    }
    moduleName.name = QByteArray((const char *)data, moduleName.cbName);
    moduleName.name.append((char)0);

    return moduleName;
}


static DBContactSettings ReadDBContactSettings(const BYTE *data,
                                               const BYTE *lastAddr)
{
    if ((lastAddr - data) < 16) {
        throw QString("invalid data format");
    }

    DBContactSettings settings;
    settings.signature = ReadDWord(data);
    settings.ofsNext = ReadDWord(data);
    settings.ofsModuleName = ReadDWord(data);
    settings.cbBlob = ReadDWord(data);
    if (settings.cbBlob > (lastAddr - data)) {
        throw QString("invalid data format");
    }
    settings.blob = QByteArray((const char *)data, settings.cbBlob);

    return settings;
}


QVariant GetVariant(const BYTE *&data, QTextDecoder *decoder)
{
    BYTE type = ReadByte(data);
    switch (type) {
    case DBVT_DELETED:
        return QVariant();
    case DBVT_BYTE:
        return ReadByte(data);
    case DBVT_WORD:
        return ReadWord(data);
    case DBVT_DWORD:
        return ReadDWord(data);
    case DBVT_ASCIIZ:
        return decoder->toUnicode(ReadByteArray(data));
    case DBVT_UTF8:
        return QString::fromUtf8(ReadByteArray(data));
    case DBVT_WCHAR: {
        WORD length = ReadWord(data);
        WCHAR *array = (WCHAR *)malloc(length * sizeof(WORD));
        for (int i = 0; i < length; i++)
            array[i] = ReadWord(data);
        QString result = QString::fromUtf16(array, length);
        free(array);
        return result;
    }
    case DBVT_BLOB:
        return ReadByteArray(data);
    default:
        return QVariant();
    }
}

static QMap<QString, QMap<QString, QVariant>> GetSettings(const DBContact &contact,
                                                          QHash<DWORD, DBContactSettings> dbContactSettings,
                                                          QHash<DWORD, DBModuleName> dbModuleNames,
                                                          QTextDecoder *decoder)
{
    DBContactSettings contact_settings;
    DWORD offset = contact.ofsFirstSettings;
    QMap<QString, QMap<QString, QVariant>> topResult;
    while (offset) {
        if (!dbContactSettings.contains(offset)) {
            break;
        }
        contact_settings = dbContactSettings[offset];
        if (dbModuleNames.contains(contact_settings.ofsModuleName)) {
            DBModuleName module_name = dbModuleNames[contact_settings.ofsModuleName];
            QMap<QString, QVariant> result;
            const BYTE *data = (const BYTE *)contact_settings.blob.constData();
            while (true) {
                BYTE length = ReadByte(data);
                QByteArray key = QByteArray((const char *)data, length);
                data += length;
                if (key.isEmpty()) {
                    break;
                }
                QVariant value = GetVariant(data, decoder);
                if (!value.isNull()) {
                    result.insert(QString::fromLatin1(key, key.size()).toLower(), value);
                }
            }

            topResult.insert(QString(module_name.name.data()), result);
        }
        offset = contact_settings.ofsNext;
    }

    return topResult;
}


inline DWORD ReadSignature(const BYTE *data)
{
    DWORD a = *(data++);
    a += (*(data++) << 8);
    a += (*(data++) << 16);
    a += (*(data++) << 24);

    return a;
}


bool miranda2json(const QString &mirandaDbFile,
                  const QString &outputJsonFile,
                  bool verbose)
{
    // for decoding russian text inside the miranda db
    QTextDecoder *decoder = QTextCodec::codecForName("CP1251")->makeDecoder();

    QFile file(mirandaDbFile);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "can't open file for read: " << mirandaDbFile.toStdString() << std::endl;
        return false;
    }

    // minimum file size
    const QByteArray bytes = file.readAll();
    if (bytes.size() < static_cast<int>(sizeof(DBHeader))) {
        std::cerr << "it's not a miranda database" << std::endl;
        return false;
    }

    // magic
    DBHeader header = ReadDBHeader((const BYTE *)bytes.constData());
    if (strcmp((const char *)header.signature, DBHEADER_SIGNATURE)) {
        std::cerr << "it's not a miranda database" << std::endl;
        return false;
    }

    // read the database structures by brutforce algorithm
    // we find the magic and try to read structure
    QHash<DWORD, DBContact> dbContacts;
    QHash<DWORD, DBEvent> dbEvents;
    QHash<DWORD, DBModuleName> dbModuleNames;
    QHash<DWORD, DBContactSettings> dbContactSettings;

    const BYTE *const firstDataAddr = (const BYTE *)bytes.constData();
    const BYTE *const lastDataAddr = firstDataAddr + bytes.size();
    const BYTE *data = firstDataAddr;
    while (lastDataAddr - data >= 4) {
        const DWORD sig = ReadSignature(data);
        const DWORD addr = (data - firstDataAddr);

        try {
            switch (sig) {
            case DBCONTACT_SIGNATURE:
                dbContacts.insert(addr, ReadDBContact(data, lastDataAddr));
                break;
            case DBEVENT_SIGNATURE:
                dbEvents.insert(addr, ReadDBEvent(data, lastDataAddr));
                break;
            case DBMODULENAME_SIGNATURE:
                dbModuleNames.insert(addr, ReadDBModuleName(data, lastDataAddr));
                break;
            case DBCONTACTSETTINGS_SIGNATURE:
                dbContactSettings.insert(addr, ReadDBContactSettings(data, lastDataAddr));
                break;
            }
        }
        catch (...) {
        }

        data += 1;
    }

    if (verbose) {
        std::cout << "== Found ==" << std::endl;
        std::cout << "  DBContact        : " << dbContacts.count()  << std::endl;
        std::cout << "  DBEvent          : " << dbEvents.count() << std::endl;
        std::cout << "  DBModuleName     : " << dbModuleNames.count() << std::endl;
        std::cout << "  DBContactSettings: " << dbContactSettings.count() << std::endl;
    }

    // create accounts own list
    QMap<QString, QVariant> accountsMap;
    accountsMap["id"] = header.ofsUser;
    QMap<QString, QMap<QString, QVariant>> userContact = GetSettings(dbContacts[header.ofsUser], dbContactSettings, dbModuleNames, decoder);
    if (userContact.contains("VKontakte")) {
        QVariantMap v;
        v["useremail"] = userContact["VKontakte"]["useremail"];
        accountsMap["vk"] = v;
    }
    if (userContact.contains("JABBER")) {
        QVariantMap v;
        v["jid"] = userContact["JABBER"]["jid"];
        accountsMap["jabber"] = v;
    }
    if (userContact.contains("ICQ")) {
        QVariantMap v;
        v["uin"] = userContact["ICQ"]["uin"];
        accountsMap["icq"] = v;
    }
    if (userContact.contains("MSN")) {
        QVariantMap v;
        v["msn"] = userContact["MSN"]["msn"];
        accountsMap["msn"] = v;
    }
    if (userContact.contains("AIM")) {
        QVariantMap v;
        v["sn"] = userContact["AIM"]["sn"];
        accountsMap["aim"] = v;
    }
    if (userContact.contains("GG")) {
        QVariantMap v;
        v["uin"] = userContact["GG"]["uin"];
        accountsMap["gg"] = v;
    }
    if (userContact.contains("IRC")) {
        QVariantMap v;
        v["nick"] = userContact["IRC"]["nick"];
        accountsMap["irc"] = v;
    }
    if (userContact.contains("YAHOO")) {
        QVariantMap v;
        v["yahoo_id"] = userContact["YAHOO"]["yahoo_id"];
        accountsMap["yahoo"] = v;
    }

    QVariantList eventList;
    foreach (const DWORD id, dbEvents.keys()) {
        const DBEvent &event = dbEvents[id];
        if (event.eventType == 0 || event.eventType == 25368) {
            QVariantMap e;
            e["id"] = id;
            e["incomming"] = !(event.flags & DBEF_SENT);
            e["prev_id"] = event.ofsPrev;
            e["next_id"] = event.ofsNext;
            e["module_name"] = dbModuleNames[event.ofsModuleName].name;
            e["timestamp"] = event.timestamp;
            if (event.flags & DBEF_UTF) {
                QString text = QString::fromUtf8(event.blob.data());
                // escape all non pritable symbols
                for (int i = 0; i < text.count(); ++i) {
                    if (text[i] == '\t') {
                        continue;
                    }
                    if (text[i] == '\n') {
                        continue;
                    }
                    if (text[i] == '\r') {
                        continue;
                    }
                    if (text[i] > 0 && text[i] <= 0x1F) {
                        text[i] = ' ';
                    }
                }
                e["text"] = text;
            }
            else {
                QByteArray data = event.blob;

                // calculate actual size of string (zero-terminated)
                int newSize = data.size();
                for (int i = 0; i < data.size(); ++i) {
                    if (data[i] == '\0') {
                        newSize = i;
                        break;
                    }
                }
                data.resize(newSize);

                // escape all non pritable symbols
                for (int i = 0; i < data.size(); ++i) {
                    if (data[i] == '\t') {
                        continue;
                    }
                    if (data[i] == '\n') {
                        continue;
                    }
                    if (data[i] == '\r') {
                        continue;
                    }
                    if (data[i] > 0 && data[i] <= 0x1F) {
                        data[i] = ' ';
                    }
                }
                e["text"] = decoder->toUnicode(data);
            }
            eventList.push_back(e);
        }
    }


    QVariantList contactList;
    foreach (const DWORD id, dbContacts.keys()) {
        const DBContact &contact = dbContacts[id];
        QVariantMap contactSettingsMap;
        QMap<QString, QMap<QString, QVariant>> contactSettings = GetSettings(dbContacts[id], dbContactSettings, dbModuleNames, decoder);
        if (contactSettings.contains("VKontakte")) {
            QVariantMap v;
            v["useremail"] = contactSettings["VKontakte"]["useremail"];
            v["id"] = contactSettings["VKontakte"]["id"];
            v["nick"] = contactSettings["VKontakte"]["nick"];
            contactSettingsMap["vk"] = v;
        }
        if (contactSettings.contains("JABBER")) {
            QVariantMap v;
            v["jid"] = contactSettings["JABBER"]["jid"];
            v["nick"] = contactSettings["JABBER"]["nick"];
            contactSettingsMap["jabber"] = v;
        }
        if (contactSettings.contains("ICQ")) {
            QVariantMap v;
            v["uin"] = contactSettings["ICQ"]["uin"];
            v["nick"] = contactSettings["ICQ"]["nick"];
            v["firstname"] = contactSettings["ICQ"]["firstname"];
            v["lastname"] = contactSettings["ICQ"]["lastname"];
            contactSettingsMap["icq"] = v;
        }
        if (contactSettings.contains("MSN")) {
            QVariantMap v;
            v["msn"] = contactSettings["MSN"]["msn"];
            contactSettingsMap["msn"] = v;
        }
        if (contactSettings.contains("AIM")) {
            QVariantMap v;
            v["sn"] = contactSettings["AIM"]["sn"];
            contactSettingsMap["aim"] = v;
        }
        if (contactSettings.contains("GG")) {
            QVariantMap v;
            v["uin"] = contactSettings["GG"]["uin"];
            contactSettingsMap["gg"] = v;
        }
        if (contactSettings.contains("IRC")) {
            QVariantMap v;
            v["nick"] = contactSettings["IRC"]["nick"];
            contactSettingsMap["irc"] = v;
        }
        if (contactSettings.contains("YAHOO")) {
            QVariantMap v;
            v["yahoo_id"] = contactSettings["YAHOO"]["yahoo_id"];
            contactSettingsMap["yahoo"] = v;
        }

        QVariantMap map;
        map["id"] = id;
        map["settings"] = contactSettingsMap;
        map["first_event_id"] = contact.ofsFirstEvent;
        map["last_event_id"] = contact.ofsLastEvent;
        map["first_unread_event_id"] = contact.ofsFirstUnreadEvent;
        map["event_count"] = contact.eventCount;
        contactList.append(map);
    }

    // write json file to the disk
    QMap<QString, QVariant> compliteMap;
    compliteMap["accounts"] = accountsMap;
    compliteMap["contacts"] = contactList;
    compliteMap["events"] = eventList;
    QFile outputFile(outputJsonFile);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        std::cerr << "can't open file for write: " << outputJsonFile.toStdString() << std::endl;
        return false;
    }

    outputFile.write(QtJson::serialize(compliteMap));
    outputFile.close();

    return false;
}

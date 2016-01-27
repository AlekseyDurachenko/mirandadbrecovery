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


void ReadDBHeader(DBHeader *header, const BYTE *data)
{
    for (int i = 0; i < 16; i++) {
        header->signature[i] = ReadByte(data);
    }
    header->version = ReadDWord(data);
    header->ofsFileEnd = ReadDWord(data);
    header->slackSpace = ReadDWord(data);
    header->contactCount = ReadDWord(data);
    header->ofsFirstContact = ReadDWord(data);
    header->ofsUser = ReadDWord(data);
    header->ofsFirstModuleName = ReadDWord(data);
}


void ReadDBContact(DBContact *contact, const BYTE *data)
{
    contact->signature = ReadDWord(data);
    contact->ofsNext = ReadDWord(data);
    contact->ofsFirstSettings = ReadDWord(data);
    contact->eventCount = ReadDWord(data);
    contact->ofsFirstEvent = ReadDWord(data);
    contact->ofsLastEvent = ReadDWord(data);
    contact->ofsFirstUnreadEvent = ReadDWord(data);
    contact->timestampFirstUnread = ReadDWord(data);
}


void ReadDBEvent(DBEvent *event, const BYTE *data)
{
    event->signature = ReadDWord(data);
    event->ofsPrev = ReadDWord(data);
    event->ofsNext = ReadDWord(data);
    event->ofsModuleName = ReadDWord(data);
    event->timestamp = ReadDWord(data);
    event->flags = ReadDWord(data);
    event->eventType = ReadWord(data);
    event->cbBlob = ReadDWord(data);
    event->blob = QByteArray((const char *)data, event->cbBlob);
}


void ReadDBModuleName(DBModuleName *module_name, const BYTE *data)
{
    module_name->signature = ReadDWord(data);
    module_name->ofsNext = ReadDWord(data);
    module_name->cbName = ReadByte(data);
    module_name->name = QByteArray((const char *)data, module_name->cbName);
    module_name->name.append((char)0);
}


void ReadDBContactSettings(DBContactSettings *contact_settings,
                           const BYTE *data)
{
    contact_settings->signature = ReadDWord(data);
    contact_settings->ofsNext = ReadDWord(data);
    contact_settings->ofsModuleName = ReadDWord(data);
    contact_settings->cbBlob = ReadDWord(data);
    contact_settings->blob = QByteArray((const char *)data, contact_settings->cbBlob);
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

QHash<QString, QVariant> GetSettings(const DBContact &contact,
                                     const QByteArray &module,
                                     const BYTE *data,
                                     QTextDecoder *decoder)
{
    DBContactSettings contact_settings;
    DWORD offset = contact.ofsFirstSettings;
    QHash<QString, QVariant> result;
    while (offset) {
        ReadDBContactSettings(&contact_settings, data + offset);
        DBModuleName module_name;
        ReadDBModuleName(&module_name, data + contact_settings.ofsModuleName);
        if (QLatin1String(module_name.name) == QLatin1String(module)) {
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
        }
        offset = contact_settings.ofsNext;
    }

    return result;
}


QString GetID(const QHash<QString, QVariant> &settings,
              const QByteArray &protocol)
{
    if (protocol.startsWith("JABBER"))
        return settings.value("jid").toString();
    if (protocol.startsWith("ICQ"))
        return settings.value("uin").toString();
    if (protocol.startsWith("MSN"))
        return settings.value("e-mail").toString();
    if (protocol.startsWith("AIM"))
        return settings.value("sn").toString();
    if (protocol.startsWith("GG"))
        return settings.value("uin").toString();
    if (protocol.startsWith("IRC"))
        return settings.value("nick").toString();
    if (protocol.startsWith("YAHOO"))
        return settings.value("yahoo_id").toString();

    return QString();
}

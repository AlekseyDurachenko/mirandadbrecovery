# mirandadbrecovery

The utility reads the broken miranda database and save recovered data into json.

__Be creaful: it works only for my case. In your case it may work with bugs and looses of the data.__

## json output file format
```
{ 
  "accounts": [
    "id": "$VALUE",
    "vk": {
      "useremail": "$VALUE"
    },
    "jabber": {
      "jid": "$VALUE"
    },
    "icq": {
      "uin": "$VALUE"
    },
    "msn": {
      "msn": "$VALUE"
    },
    "aim": {
      "sn": "$VALUE"
    },
    "gg": {
      "uin": "$VALUE"
    },
    "irc": {
      "nick": "$VALUE"
    },
    "yahoo": {
      "yahoo_id": "$VALUE"
    }    
  ],
  "contacts": [
    {
      "id": "$VALUE",
      "first_event_id": "$VALUE",
      "last_event_id": "$VALUE",
      "first_unread_event_id": "$VALUE",
      "event_count": "$VALUE",
      "settings": {
        "vk": {
          "useremail": "$VALUE",
          "id": "$VALUE",
          "nick": "$VALUE"
        },
        "jabber": {
          "jid": "$VALUE",
          "nick": "$VALUE"
        },
        "icq": {
          "uin": "$VALUE",
          "nick": "$VALUE",
          "firstname": "$VALUE",
          "lastname": "$VALUE"
        },
        "msn": {
          "msn": "$VALUE"
        },
        "aim": {
          "sn": "$VALUE"
        },
        "gg": {
          "uin": "$VALUE"
        },
        "irc": {
          "nick": "$VALUE"
        },
        "yahoo": {
          "yahoo_id": "$VALUE"
        }        
      }
    }
  ],
  "events": [
    {
      "id": "$VALUE",
      "incomming": "$VALUE",
      "prev_id": "$VALUE",
      "next_id": "$VALUE",
      "module_name": "$VALUE",
      "timestamp": "$VALUE",
      "text": "$VALUE"
    },
    ...
  ],  
}
```

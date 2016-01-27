unix {
    docs.files      =                                               \
        $$PWD/AUTHORS                                               \
        $$PWD/CHANGELOG                                             \
        $$PWD/LIBRARIES                                             \
        $$PWD/LICENSE                                               \
        $$PWD/LICENSE.GPL-3+                                        \

    contains(DEFINES, APP_PORTABLE) {
        INSTALLS += target docs

        target.path     = /
        docs.path       = /
    }

    !contains(DEFINES, APP_PORTABLE) {
        INSTALLS += target docs

        target.path         = /usr/bin/
        docs.path           = /usr/share/$${TARGET}

        # you should to determine the location of resources
        DEFINES += "APP_RESOURCES_PREFIX='/usr/share/$${TARGET}'"
    }
}

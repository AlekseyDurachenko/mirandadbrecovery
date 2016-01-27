INCLUDEPATH    +=                                                       \
    $$PWD/src                                                           \


HEADERS        +=                                                       \
    $$PWD/src/miranda.h                                                 \


SOURCES        +=                                                       \
    $$PWD/src/miranda.cpp                                               \


FORMS          +=                                                       \


TRANSLATIONS   +=                                                       \


RESOURCES      +=                                                       \


OTHER_FILES    +=                                                       \
    $$PWD/AUTHORS                                                       \
    $$PWD/CHANGELOG                                                     \
    $$PWD/LIBRARIES                                                     \
    $$PWD/LICENSE                                                       \
    $$PWD/LICENSE.GPL-3+                                                \
    $$PWD/README.md                                                     \


!contains(QT, testlib) {
    HEADERS   +=                                                        \

    SOURCES   +=                                                        \
        $$PWD/src/main.cpp                                              \

}

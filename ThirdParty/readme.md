MSVC generation
---------------

1. Generate the project files for zlib into zlib/build with CMake

2. Build zlib (both debug and release)

3. Generate the project files for quazip into quazip/build with CMake by setting the following

        Qt5Core_DIR = D:\Qt\Qt5.7.0\5.7\msvc2015_64\lib\cmake\Qt5Core (or wherever you installed the MSVC copy of Qt)

4. Add zlib include path and .lib files (dynamic linking)

        ZLIB_INCLUDE_DIR=C:\...eshare\ThirdParty\zlib;C:\...eshare\ThirdParty\zlib\build
        ZLIB_LIBRARY_DEBUG=C:\...eshare\ThirdParty\zlib\build\Debug\zlibd.lib
        ZLIB_LIBRARY_RELEASE=C:\...eshare\ThirdParty\zlib\build\Release\zlib.lib

5. Build quazip (both debug and release)

6. Add the following to the pro file

        # zlib and quazip
        INCLUDEPATH += $$PWD/ThirdParty/zlib \
                       $$PWD/ThirdParty/zlib/build
        INCLUDEPATH += ThirdParty/quazip/quazip
        debug {
            LIBS += -L$$PWD/ThirdParty/zlib/build/Debug -lzlibd
            LIBS += -L$$PWD/ThirdParty/quazip/build/Debug -lquazip
        }
        release {
            LIBS += -L$$PWD/ThirdParty/zlib/build/Release -lzlib
            LIBS += -L$$PWD/ThirdParty/quazip/build/Release -lquazip
        }

@rem builds a #define with the build date and time.
@rem We could add a git hash with: for /f %i in ('git rev-parse --short HEAD') do set hash=%i
@echo on
@setlocal
@rem format current date time as  yyyy-mm-dd hh:mm:ss
@set dt=%DATE:~10,4%-%DATE:~4,2%-%DATE:~7,2% %TIME:~0,2%:%TIME:~3,2%:%TIME:~6,2%
@set VERSION_STRING=V3.1 of %dt%
echo #define BUILD_VERSION "%VERSION_STRING%">src/build_version.h
echo %VERSION_STRING%>Objects/firmware_built.txt

@rem builds a #define with the build date and time.
@rem We could add a git hash with: for /f %i in ('git rev-parse --short HEAD') do set hash=%i
@echo on
@setlocal
@set VERSION_STRING=V3.1 of %DATE% %TIME%
echo #define BUILD_VERSION "%VERSION_STRING%">src/build_version.h
echo %VERSION_STRING%>Objects/firmware_built.txt
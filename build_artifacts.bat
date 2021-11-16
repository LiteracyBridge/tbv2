@rem Copy build artifacts to the Artifacts directory
@rem Although some windows commands know how to spell filenames with "/", the
@rem   copy command is not one of them.
if not exist Artifacts mkdir Artifacts
copy Objects\firmware_built.txt Artifacts\
copy Objects\TBookRev2b.hex Artifacts\
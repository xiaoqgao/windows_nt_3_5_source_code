obj\$(TARGET_DIRECTORY)\addfile.exe: obj\$(TARGET_DIRECTORY)\execslm.exe
    copy /v $** $@
    binplace $@

obj\$(TARGET_DIRECTORY)\addproj.exe: obj\$(TARGET_DIRECTORY)\execslm.exe
    copy /v $** $@
    binplace $@

obj\$(TARGET_DIRECTORY)\catsrc.exe: obj\$(TARGET_DIRECTORY)\execslm.exe
    copy /v $** $@
    binplace $@

obj\$(TARGET_DIRECTORY)\defect.exe: obj\$(TARGET_DIRECTORY)\execslm.exe
    copy /v $** $@
    binplace $@

obj\$(TARGET_DIRECTORY)\delfile.exe: obj\$(TARGET_DIRECTORY)\execslm.exe
    copy /v $** $@
    binplace $@

obj\$(TARGET_DIRECTORY)\delproj.exe: obj\$(TARGET_DIRECTORY)\execslm.exe
    copy /v $** $@
    binplace $@

obj\$(TARGET_DIRECTORY)\enlist.exe: obj\$(TARGET_DIRECTORY)\execslm.exe
    copy /v $** $@
    binplace $@

obj\$(TARGET_DIRECTORY)\in.exe: obj\$(TARGET_DIRECTORY)\execslm.exe
    copy /v $** $@
    binplace $@

obj\$(TARGET_DIRECTORY)\log.exe: obj\$(TARGET_DIRECTORY)\execslm.exe
    copy /v $** $@
    binplace $@

obj\$(TARGET_DIRECTORY)\out.exe: obj\$(TARGET_DIRECTORY)\execslm.exe
    copy /v $** $@
    binplace $@

obj\$(TARGET_DIRECTORY)\scomp.exe: obj\$(TARGET_DIRECTORY)\execslm.exe
    copy /v $** $@
    binplace $@

obj\$(TARGET_DIRECTORY)\ssync.exe: obj\$(TARGET_DIRECTORY)\execslm.exe
    copy /v $** $@
    binplace $@

obj\$(TARGET_DIRECTORY)\status.exe: obj\$(TARGET_DIRECTORY)\execslm.exe
    copy /v $** $@
    binplace $@

_LNG=$(LANGUAGE)
_INX=.
STAMP=stampinf -f $@ -a $(_BUILDARCH) -k $(KMDF_VERSION_MAJOR).$(KMDF_VERSION_MINOR)



$(OBJ_PATH)\$O\$(INF_NAME).inf: $(_INX)\$(INF_NAME).inx
    copy $(_INX)\$(@B).inx $@
    $(STAMP)

$(OBJ_PATH)\$(O)\fx2Events.h: osrusbfx2.man
     $(MC) -km -mof -h $(OBJ_PATH)\$(O) -r $(OBJ_PATH)\$(O) -z fx2Events -b $**

